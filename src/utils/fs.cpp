#include "fs.hpp"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace utils::filesystem {

std::vector<std::string> list_json_files(const std::string& dir_path) {
    std::vector<std::string> files;

    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                files.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error listing JSON files: " + std::string(e.what()));
    }

    return files;
}

bool create_symlink(const std::string& target, const std::string& link_path) {
    try {
        fs::remove(link_path);  // 统一先删除旧链接（Windows + Linux）

#ifdef _WIN32
        DWORD attr = GetFileAttributesA(target.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;

        bool is_dir = attr & FILE_ATTRIBUTE_DIRECTORY;
        return CreateSymbolicLinkA(link_path.c_str(), target.c_str(),
                                   is_dir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
#else
        fs::create_symlink(target, link_path);
        return true;
#endif
    } catch (const std::exception& e) {
        return false;
    }
}

std::string read_symlink(const std::string& link_path) {
    try {
#ifdef _WIN32
        HANDLE hFile = CreateFileA(link_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return "";
        }

        char buffer[MAX_PATH];
        DWORD result = GetFinalPathNameByHandleA(hFile, buffer, MAX_PATH, FILE_NAME_NORMALIZED);
        CloseHandle(hFile);

        if (result == 0 || result >= MAX_PATH) return "";

        // Windows 返回路径格式为 "\\?\" 开头，需要清理
        std::string fullPath = buffer;
        const std::string prefix = R"(\\?\)";
        if (fullPath.rfind(prefix, 0) == 0) {
            fullPath = fullPath.substr(prefix.length());
        }

        return fullPath;
#else
        if (fs::is_symlink(link_path)) {
            return fs::read_symlink(link_path).string();
        }
        return "";
#endif
    } catch (const std::exception& e) {
        throw std::runtime_error("Error reading symlink: " + std::string(e.what()));
    }
}

bool is_symlink(const std::string& path) {
    try {
#ifdef _WIN32
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        return (attr & FILE_ATTRIBUTE_REPARSE_POINT);
#else
        return fs::is_symlink(path);
#endif
    } catch (...) {
        return false;
    }
}

bool remove_symlink(const std::string& link_path) {
    try {
        if (is_symlink(link_path)) {
            return fs::remove(link_path);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error removing symlink: " + std::string(e.what()));
    }
    return false;
}

}  // namespace utils::filesystem
