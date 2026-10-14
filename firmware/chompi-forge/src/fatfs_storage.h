#pragma once
#include "daisy_seed.h"
#include "fatfs.h"
#include "diskio.h"
#include "../core/preset_store.h"
#include "../core/sample_loader.h"
#include "../core/file_transfer.h"

// forge::Storage on the CHOMPI SD card via libDaisy FatFS. Main loop only.
// Writes go to FORGE/TMP.FPR, are synced, then renamed over the slot file, so
// a torn write can only affect the slot being saved (and its CRC rejects it).
class FatFsStorage : public forge::Storage {
public:
    void SetMounted(bool mounted) { mounted_ = mounted; }
#ifdef FORGE_TEST_HOOKS
    bool Mounted() const { return mounted_; }
#endif
    bool Ready() override { return mounted_ && disk_status(0) == RES_OK; }
    bool Read(const char* path, uint8_t* buffer, size_t capacity, size_t& size) override {
        if(!Ready() || f_open(&file_, path, FA_OPEN_EXISTING | FA_READ) != FR_OK) return false;
        UINT read = 0;
        const bool ok = f_read(&file_, buffer, static_cast<UINT>(capacity), &read) == FR_OK;
        f_close(&file_);
        size = read;
        return ok && read < capacity;      // a file filling the buffer is too long to be a record
    }
    bool Write(const char* path, const uint8_t* data, size_t size) override {
        if(!Ready()) return false;
        f_mkdir("FORGE");                  // FR_EXIST is fine
        static const char kTemp[] = "FORGE/TMP.FPR";
        if(f_open(&file_, kTemp, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return false;
        UINT written = 0;
        bool ok = f_write(&file_, data, static_cast<UINT>(size), &written) == FR_OK && written == size;
        ok = f_sync(&file_) == FR_OK && ok;
        f_close(&file_);
        if(!ok) { f_unlink(kTemp); return false; }
        f_unlink(path);                    // FatFS rename does not replace
        return f_rename(kTemp, path) == FR_OK;
    }
    bool Remove(const char* path) override {
        if(!Ready()) return false;
        const FRESULT result = f_unlink(path);
        return result == FR_OK || result == FR_NO_FILE;
    }
private:
    FIL file_;
    bool mounted_ = false;
};

// forge::UploadFiles: USB file transfer on the same mounted card (main loop
// only), with its own write handle so it never disturbs a sample job.
class FatFsUploadFiles : public forge::UploadFiles {
public:
    explicit FatFsUploadFiles(FatFsStorage& mount) : mount_(mount) {}
    bool Ready() override { return mount_.Ready(); }
    bool Open(const char* temp) override {
        if(!Ready()) return false;
        if(open_) { f_close(&file_); open_ = false; }
        f_mkdir("FORGE");                  // FR_EXIST is fine
        open_ = f_open(&file_, temp, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK;
        return open_;
    }
    bool Append(const uint8_t* data, uint32_t size) override {
        UINT written = 0;
        return open_ && f_write(&file_, data, size, &written) == FR_OK && written == size;
    }
    FORGE_COLD bool Finish(const char* temp, const char* final_path, bool verify, uint32_t size, uint32_t crc) override {
        if(!open_) return false;
        open_ = false;
        const bool synced = f_sync(&file_) == FR_OK;
        f_close(&file_);
        if(!synced || (verify && !ReadsBack(temp, size, crc))) { f_unlink(temp); return false; }
        f_unlink(final_path);              // FatFS rename does not replace
        return f_rename(temp, final_path) == FR_OK;
    }
    void Abort(const char* temp) override {
        if(open_) { f_close(&file_); open_ = false; }
        f_unlink(temp);
    }
    bool Exists(const char* path) override { FILINFO info; return Ready() && f_stat(path, &info) == FR_OK; }
    // The bootloader takes the first visible root name containing ".bin"/".BIN": rename
    // all but FORGE.bin (a batch per directory pass, renaming after the listing).
    FORGE_COLD bool SetAsideOtherFirmware() override {
        if(!Ready()) return false;
        for(unsigned pass = 0; pass < 16; ++pass) {
            DIR dir; FILINFO info;
            if(f_opendir(&dir, "/") != FR_OK) return false;
            unsigned count = 0;
            while(count < kAsideBatch && f_readdir(&dir, &info) == FR_OK && info.fname[0]) {
                if(info.fattrib & (AM_DIR | AM_HID) || forge::IsFirmwareName(info.fname) || !forge::BootloaderMatches(info.fname)) continue;
                const size_t n = std::strlen(info.fname);
                if(n >= sizeof aside_[0]) { f_closedir(&dir); return false; }
                std::memcpy(aside_[count++], info.fname, n + 1);
            }
            f_closedir(&dir);
            if(!count) return true;        // FORGE.bin is the only image the bootloader can see
            for(unsigned i = 0; i < count; ++i) {
                bool renamed = false;
                for(unsigned attempt = 0; attempt <= 9 && !renamed; ++attempt) {
                    if(!forge::SetAsideName(aside_[i], attempt, target_, sizeof target_)) return false;
                    if(f_stat(target_, &info) == FR_OK) continue;        // never overwrite a file
                    renamed = f_rename(aside_[i], target_) == FR_OK;
                    if(!renamed) return false;
                }
                if(!renamed) return false;
            }
        }
        return false;
    }
private:
    static constexpr unsigned kAsideBatch = 8;
    // Read the closed file back (size and CRC-32). The buffer is a member of a global (.bss,
    // D1 SRAM) and cache-line aligned: whole-sector reads go straight to it by DMA.
    bool ReadsBack(const char* path, uint32_t size, uint32_t crc) {
        if(f_open(&check_, path, FA_OPEN_EXISTING | FA_READ) != FR_OK) return false;
        bool ok = f_size(&check_) == size;
        uint32_t got = 0, sum = 0;
        while(ok && got < size) {
            UINT read = 0;
            ok = f_read(&check_, verify_buffer_, sizeof verify_buffer_, &read) == FR_OK && read > 0;
            if(ok) { sum = forge::Crc32(sum, verify_buffer_, read); got += read; }
        }
        f_close(&check_);
        return ok && got == size && sum == crc;
    }
    alignas(32) uint8_t verify_buffer_[4096];
    char aside_[kAsideBatch][256], target_[268];
    FatFsStorage& mount_;
    FIL file_, check_;
    bool open_ = false;
};

// forge::SampleFiles on the same mounted card (main loop only): one read and
// one write handle, root listing for TAPE's sample names. Callers' buffers
// must be DMA-reachable (D1 SRAM, not DTCM).
class FatFsSampleFiles : public forge::SampleFiles {
public:
    explicit FatFsSampleFiles(FatFsStorage& mount) : mount_(mount) {}
    bool Ready() override { return mount_.Ready(); }
    bool OpenRead(const char* path, uint32_t& size) override {
        if(!Ready() || reading_ || f_open(&read_, path, FA_OPEN_EXISTING | FA_READ) != FR_OK) return false;
        reading_ = true; size = static_cast<uint32_t>(f_size(&read_));
        return true;
    }
    bool ReadAt(uint32_t offset, uint8_t* buffer, uint32_t size, uint32_t& got) override {
        if(!reading_ || (f_tell(&read_) != offset && f_lseek(&read_, offset) != FR_OK)) return false;
        UINT read = 0;
        if(f_read(&read_, buffer, size, &read) != FR_OK) return false;
        got = read;
        return true;
    }
    void CloseRead() override { if(reading_) { f_close(&read_); reading_ = false; } }
    bool OpenWrite(const char* temp) override {
        if(!Ready() || writing_ || f_open(&write_, temp, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return false;
        writing_ = true;
        return true;
    }
    bool Append(const uint8_t* data, uint32_t size) override {
        UINT written = 0;
        return writing_ && f_write(&write_, data, size, &written) == FR_OK && written == size;
    }
    bool FinishWrite(const char* temp, const char* final_path) override {
        if(!writing_) return false;
        writing_ = false;
        const bool synced = f_sync(&write_) == FR_OK;
        f_close(&write_);
        if(!synced) { f_unlink(temp); return false; }
        f_unlink(final_path);              // FatFS rename does not replace
        return f_rename(temp, final_path) == FR_OK;
    }
    void AbortWrite(const char* temp) override {
        if(writing_) { f_close(&write_); writing_ = false; }
        f_unlink(temp);
    }
    bool Remove(const char* path) override {
        if(!Ready()) return false;
        const FRESULT result = f_unlink(path);
        return result == FR_OK || result == FR_NO_FILE;
    }
    bool ListRoot(void (*visit)(void*, const char*), void* context) override {
        DIR dir; FILINFO info;
        if(!Ready() || f_opendir(&dir, "/") != FR_OK) return false;
        while(f_readdir(&dir, &info) == FR_OK && info.fname[0])
            if(!(info.fattrib & (AM_DIR | AM_HID))) visit(context, info.fname);
        f_closedir(&dir);
        return true;
    }
private:
    FatFsStorage& mount_;
    FIL read_, write_;
    bool reading_ = false, writing_ = false;
};
