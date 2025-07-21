#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace config {

    using json = nlohmann::ordered_json;

    // 设置默认配置目录（程序启动时调用一次）
    void set_default_config_dir(const std::string& path);

    // 获取默认配置目录（调用前需先调用 set_default_config_dir）
    const std::string& get_default_config_dir();

    // 根据应用名自动检测默认配置目录（内部辅助函数，一般 main 调用后传入 set_default_config_dir）
    std::string detect_default_config_dir(const std::string& app_name);

    // 加载指定路径的配置文件（json格式）
    json load_config(const std::string& path);

    // 保存配置到指定路径（json格式）
    void save_config(const std::string& path, const json& config);

    // 获取“激活”的配置文件路径（即 active 符号链接指向的文件）
    std::string get_active_config_path();

    // 设置“激活”的配置文件（通过更新 active 符号链接）
    void set_active_config(const std::string& config_path);

    // 验证 schema.json 是否存在
    bool has_schema();

    // 从指定位置复制 schema.json
    void copy_schema_to_default_dir(const std::string& from_path);

} // namespace config
