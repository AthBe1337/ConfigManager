#include <iostream>
#include "config.h"
#include "ui/init.hpp"
#include "ui/main_ui.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    try {

        std::string app_name;

        // 1. 获取应用名
        if (argc > 1) {
            app_name = argv[1];
        } else {
            app_name = ui::ask_app_name();  // 需在 ui::file_selector.cpp 或 utils 添加交互
        }

        // 2. 设置配置目录
        std::string config_path = config::detect_default_config_dir(app_name);
        config::set_default_config_dir(config_path);

        // 3. 检查 schema
        if (!config::has_schema()) {
            std::string path = ui::ask_schema_path();  // 弹窗输入路径
            config::copy_schema_to_default_dir(path);  // 实现复制到 config_path/../schema.json
        }

        // 4. 加载 schema
        auto schema = config::load_schema(fs::path(config_path).parent_path() / "schema.json");

        // 5. 校验当前激活配置（如果存在）
        if (fs::exists(config::get_active_config_path())) {
            auto cfg = config::load_config(config::get_active_config_path());
            try {
                config::validate_config(cfg, schema);
            } catch (const std::exception& e) {
                fs::remove(config::get_active_config_path());
                ui::show_warning_message("激活配置校验失败",
                    "已激活的配置文件无法通过 schema 校验，已取消激活。");
            }
        }

        // 6. 启动 UI 主界面
        ui::run_main_ui(app_name, schema);

    } catch (const std::exception& e) {
        std::cerr << "启动失败: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

