#include <string>
#include "../config.h"


namespace ui {
    bool confirm_dialog(const std::string& title, const std::string& message);

    // 显示警告对话框
    void show_warning(const std::string& title, const std::string& message);

    // 获取新文件名（无扩展名）
    std::string ask_new_filename(const std::vector<std::string>& existing);

    void run_main_ui(const std::string& app_name, const config::json& schema);
}