#ifndef MEMORY_FILESYSTEM_H
#define MEMORY_FILESYSTEM_H

#include <stdexcept>
#include <string>
#include <unordered_map>

// 웹 환경용 인메모리 파일시스템
class MemoryFileSystem {
public:
    void WriteFile(const std::string& path, const std::string& content) {
        files[path] = content;
    }

    std::string ReadFile(const std::string& path) const {
        auto it = files.find(path);
        if (it == files.end()) {
            throw std::runtime_error("파일을 찾을 수 없습니다: " + path);
        }
        return it->second;
    }

    bool FileExists(const std::string& path) const {
        return files.count(path) > 0;
    }

    void DeleteFile(const std::string& path) {
        files.erase(path);
    }

    void Clear() {
        files.clear();
    }

private:
    std::unordered_map<std::string, std::string> files;
};

#endif // MEMORY_FILESYSTEM_H
