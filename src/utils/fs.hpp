#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace utils::filesystem {

    // 列出目录下所有 *.json 文件（不含子目录）
    std::vector<std::string> list_json_files(const std::string& dir_path);

    // 创建符号链接（active → target）
    bool create_symlink(const std::string& target, const std::string& link_path);

    // 读取符号链接指向的目标路径（返回空字符串表示失败或非符号链接）
    std::string read_symlink(const std::string& link_path);

    // 判断是否是符号链接
    bool is_symlink(const std::string& path);

    // 删除符号链接（不会删除目标文件）
    bool remove_symlink(const std::string& link_path);

}  // namespace utils::fs
