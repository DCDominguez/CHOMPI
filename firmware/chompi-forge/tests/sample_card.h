#pragma once
// In-memory SD card implementing forge::SampleFiles (host tests and forge_probe).
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>
#include "../core/sample_loader.h"

class SampleCard : public forge::SampleFiles {
public:
    std::map<std::string, std::vector<uint8_t>> files;
    bool ready = true, fail_reads = false, fail_writes = false;
    long append_budget = -1;   // bytes Append accepts before failing (-1 = unlimited)
    unsigned reads = 0, opens = 0;
    bool Ready() override { return ready; }
    bool OpenRead(const char* path, uint32_t& size) override {
        auto it = Find(path);
        if(!ready || it == files.end() || reading_) return false;
        reading_ = true; read_name_ = it->first; size = static_cast<uint32_t>(it->second.size()); ++opens;
        return true;
    }
    bool ReadAt(uint32_t offset, uint8_t* buffer, uint32_t size, uint32_t& got) override {
        if(!ready || !reading_ || fail_reads) return false;
        const auto& data = files[read_name_];
        got = offset >= data.size() ? 0 : static_cast<uint32_t>(std::min<size_t>(size, data.size() - offset));
        std::copy(data.begin() + std::min<size_t>(offset, data.size()), data.begin() + std::min<size_t>(offset, data.size()) + got, buffer);
        ++reads;
        return true;
    }
    void CloseRead() override { reading_ = false; }
    bool OpenWrite(const char* temp) override {
        if(!ready || writing_ || fail_writes) return false;
        writing_ = true; files[temp].clear();
        return true;
    }
    bool Append(const uint8_t* data, uint32_t size) override {
        if(!ready || !writing_ || fail_writes) return false;
        if(append_budget >= 0) { if(long(size) > append_budget) return false; append_budget -= long(size); }
        for(auto& f : files) if(f.first == kTempName) { f.second.insert(f.second.end(), data, data + size); return true; }
        return false;
    }
    bool FinishWrite(const char* temp, const char* final_path) override {
        if(!ready || !writing_) return false;
        writing_ = false;
        auto it = files.find(temp);
        if(it == files.end()) return false;
        auto existing = Find(final_path);
        if(existing != files.end()) files.erase(existing);
        files[final_path] = it->second; files.erase(temp);
        return true;
    }
    void AbortWrite(const char* temp) override { writing_ = false; files.erase(temp); }
    bool Remove(const char* path) override {
        if(!ready) return false;
        auto it = Find(path);
        if(it != files.end()) files.erase(it);
        return true;
    }
    bool ListRoot(void (*visit)(void*, const char*), void* context) override {
        if(!ready) return false;
        for(const auto& f : files) visit(context, f.first.c_str());
        return true;
    }
    bool Has(const std::string& name) { return Find(name.c_str()) != files.end(); }
    std::vector<uint8_t>& Get(const std::string& name) { return Find(name.c_str())->second; }
private:
    static constexpr const char* kTempName = "FORGE_TMP.WAV";
    // FAT names are case-insensitive.
    std::map<std::string, std::vector<uint8_t>>::iterator Find(const char* path) {
        std::string want(path);
        for(auto& c : want) c = static_cast<char>(std::tolower(c));
        for(auto it = files.begin(); it != files.end(); ++it) {
            std::string name = it->first;
            for(auto& c : name) c = static_cast<char>(std::tolower(c));
            if(name == want) return it;
        }
        return files.end();
    }
    bool reading_ = false, writing_ = false;
    std::string read_name_;
};
