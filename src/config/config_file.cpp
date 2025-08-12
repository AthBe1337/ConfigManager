#include "config_file.hpp"
#include "../utils/fs.hpp"
#include <fstream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>

std::string get_env_var(const std::string& var) {
    char buffer[32767]; // Windows 环境变量最大长度限制
    DWORD ret = GetEnvironmentVariableA(var.c_str(), buffer, sizeof(buffer));
    if (ret == 0 || ret >= sizeof(buffer)) {
        return ""; // 未设置或错误
    }
    return std::string(buffer);
}
#endif

namespace config {

    namespace fs = std::filesystem;

    static std::string default_config_dir;  // 静态保存默认配置目录

    void set_default_config_dir(const std::string& path) {
        default_config_dir = path;
        try {
            if (!fs::exists(default_config_dir)) {
                fs::create_directories(default_config_dir);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to create default config dir: " + path + ", error: " + e.what());
        }
    }

    const std::string& get_default_config_dir() {
        if (default_config_dir.empty()) {
            throw std::runtime_error("Default config dir is not set. Call set_default_config_dir() first.");
        }
        return default_config_dir;
    }

    std::string configs_dir() {
        return get_default_config_dir();  // configs 目录就是默认配置目录
    }

    std::string active_link_path() {
        return (fs::path(get_default_config_dir()) / "active").string();
    }

    std::string detect_default_config_dir(const std::string& app_name) {
    #ifdef _WIN32
        std::string appdata = get_env_var("APPDATA");
        std::string path;
        if (!appdata.empty()) {
            path = appdata + "\\" + app_name + "\\configs";
        } else {
            char userprofile[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userprofile))) {
                path = std::string(userprofile) + "\\AppData\\Roaming\\" + app_name + "\\configs";
            } else {
                path = ".\\configs";
            }
        }
    #else
        const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
        std::string base;
        if (xdg_config_home && xdg_config_home[0] != '\0') {
            base = xdg_config_home;
        } else {
            const char* home = std::getenv("HOME");
            if (home && home[0] != '\0') {
                base = std::string(home) + "/.config";
            } else {
                base = ".";
            }
        }
        std::string path = base + "/" + app_name + "/configs";
    #endif

        return path;
    }

    json load_config(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            throw std::runtime_error("Cannot open config file: " + path);
        }
        json j;
        try {
            ifs >> j;
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to parse config JSON: " + std::string(e.what()));
        }
        return j;
    }

    void save_config(const std::string& path, const json& config) {
        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot open config file for writing: " + path);
        }
        ofs << config.dump(4);
        ofs.close();
    }

    std::string get_active_config_path() {
        std::string target = utils::filesystem::read_symlink(active_link_path());
        if (target.empty()) {
            throw std::runtime_error("Failed to read active symlink or symlink does not exist: " + active_link_path());
        }
        return target;
    }

    void set_active_config(const std::string& config_path) {
        if (!fs::exists(config_path)) {
            throw std::runtime_error("Config file does not exist: " + config_path);
        }
        if (utils::filesystem::is_symlink(active_link_path())) {
            utils::filesystem::remove_symlink(active_link_path());
        }
        bool ok = utils::filesystem::create_symlink(config_path, active_link_path());
        if (!ok) {
            throw std::runtime_error("Failed to create symlink: " + active_link_path() + " -> " + config_path);
        }
    }

    bool has_schema() {
        // 检测config目录的上级目录下是否有schema.json文件
        return fs::is_regular_file(fs::path(config::configs_dir()).parent_path() / "schema.json");
    }

    void copy_schema_to_default_dir(const std::string& from_path) {
        // 检查from_path是否存在且为文件
        if (!fs::exists(from_path) || !fs::is_regular_file(from_path)) {
            throw std::runtime_error("Schema file does not exist: " + from_path);
        }
        // 直接复制，因为configs_dir会检测
        try {
            fs::copy_file(from_path, fs::path(config::configs_dir()).parent_path() / "schema.json");
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to copy schema file: " + std::string(e.what()));
        }

    }

    void remove_active_config_link() {
        try {
            if (fs::exists(active_link_path()) && utils::filesystem::is_symlink(active_link_path())) {
                fs::remove(active_link_path());
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to remove active symlink: " + std::string(e.what()));
        }
    }


} // namespace config
