#pragma once
#include <string>

namespace ui {
    std::string ask_input_blocking(const std::string& title, const std::string& placeholder);
    std::string ask_app_name();
    std::string ask_schema_path();
    void show_warning_message(const std::string& title, const std::string& message);
}