#pragma once
#include "daisy_seed.h"
#include "fatfs.h"
#include "diskio.h"
#include "../core/preset_store.h"
#include "../core/sample_loader.h"

// forge::Storage on the CHOMPI SD card via libDaisy FatFS. Main loop only.
// Writes go to FORGE/TMP.FPR, are synced, then renamed over the slot file, so
// a torn write can only affect the slot being saved (and its CRC rejects it).
class FatFsStorage : public forge::Storage {
public:
    void SetMounted(bool mounted) { mounted_ = mounted; }
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
