#pragma once
#include "daisy_seed.h"
#include "fatfs.h"
#include "diskio.h"
#include "../core/preset_store.h"

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
