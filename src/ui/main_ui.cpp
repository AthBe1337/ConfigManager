#include "../utils/fs.hpp"
#include "main_ui.hpp"
#include "edit.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <ctime>

using namespace ftxui;
namespace fs = std::filesystem;

namespace ui {

    // 显示确认对话框
    bool confirm_dialog(const std::string& title, const std::string& message) {
        bool confirmed = false;
        auto screen = ScreenInteractive::FitComponent();

        auto confirm_button = Button("确定", [&] {
            confirmed = true;
            screen.Exit();
        });
        auto cancel_button = Button("取消", [&] {
            confirmed = false;
            screen.Exit();
        });

        auto layout = Container::Horizontal({confirm_button, cancel_button});
        auto renderer = Renderer(layout, [&] {
            return vbox({
                text(title) | bold,
                separator(),
                text(message),
                separator(),
                hbox({confirm_button->Render(), cancel_button->Render()}) | center
            }) | border | center;
        });

        screen.Loop(renderer);
        return confirmed;
    }

    // 显示警告对话框
    void show_warning(const std::string& title, const std::string& message) {
        auto screen = ScreenInteractive::FitComponent();
        auto confirm_button = Button("确定", [&] { screen.Exit(); });

        auto layout = Container::Vertical({confirm_button});
        auto renderer = Renderer(layout, [&] {
            return vbox({
                text(title) | bold | color(Color::Red),
                separator(),
                text(message),
                separator(),
                confirm_button->Render() | center
            }) | border | center;
        });

        screen.Loop(renderer);
    }

    // 获取新文件名（无扩展名）
    std::string ask_new_filename(const std::vector<std::string>& existing) {
        std::string input;
        std::string result;
        std::string error_message;
        std::regex valid_name(R"(^[A-Za-z0-9_-]+$)");

        auto screen = ScreenInteractive::FitComponent();
        auto input_box = Input(&input, "不需要扩展名");
        auto confirm = Button("确定", [&] {
            if (input.empty()) {
                error_message = "文件名不能为空";
            } else if (!std::regex_match(input, valid_name)) {
                error_message = "包含非法字符 (仅限字母数字、- 和 _)";
            } else if (std::find(existing.begin(), existing.end(), input + ".json") != existing.end()) {
                error_message = "文件已存在";
            } else {
                result = input + ".json";
                screen.Exit();
            }
        });

        auto layout = Container::Vertical({input_box, confirm});
        auto renderer = Renderer(layout, [&] {
            Elements elements = {
                text("请输入新配置文件名") | bold,
                input_box->Render(),
                confirm->Render()
            };
            if (!error_message.empty()) {
                elements.push_back(text(error_message) | color(Color::Red));
            }
            return vbox(elements) | border | center;
        });

        screen.Loop(renderer);
        return result;
    }

    void run_main_ui(const std::string& app_name, const config::json& schema) {
        bool refresh = true;
        while (refresh) {
            refresh = false;
            auto screen = ScreenInteractive::Fullscreen();

            std::string config_dir = config::get_default_config_dir();
            auto files = utils::filesystem::list_json_files(config_dir);
            std::string active;
            try {
                active = fs::path(config::get_active_config_path()).filename().string();
            } catch (const std::exception& e) {
                // 忽略
            }

            std::vector<std::string> display_files;
            for (const auto& f : files) {
                display_files.push_back((f == active ? "* " : "  ") + f);
            }

            int selected = 0;

            auto on_edit = [&] {
                if (files.empty()) return;
                screen.Exit();
                ui::edit_config(config_dir + "/" + files[selected], app_name, schema);
            };

            auto on_delete = [&] {
                if (files.empty()) return;
                std::string target = config_dir + "/" + files[selected];
                if (confirm_dialog("确认删除", "是否删除配置文件：" + files[selected] + "？")) {
                    std::string active_config;
                    try {
                        active_config = config::get_active_config_path();
                    } catch (const std::exception& e) {
                        // 忽略
                    }
                    if (!active_config.empty() && fs::exists(active_config) &&
                        fs::equivalent(target, active_config)) {
                        fs::remove(config::get_active_config_path());
                    }
                    fs::remove(target);
                    refresh = true;
                    screen.Exit();
                }
            };

            auto on_activate = [&] {
                if (files.empty()) return;
                if (confirm_dialog("确认激活", "是否设为激活配置：" + files[selected] + "？")) {
                    std::string target_path = config_dir + "/" + files[selected];
                    try {
                        auto cfg = config::load_config(target_path);
                        config::validate_config(cfg, schema);
                        config::set_active_config(target_path);
                    } catch (const std::exception& e) {
                        show_warning("校验失败", "配置未通过校验，请仔细检查\n错误信息: " + std::string(e.what()));
                    }
                    refresh = true;
                    screen.Exit();
                }
            };

            auto on_create = [&] {
                std::string filename = ask_new_filename(files);
                if (!filename.empty()) {
                    config::json new_config = config::generate_default_config(schema);
                    config::save_config(config_dir + "/" + filename, new_config);
                    refresh = true;
                    screen.Exit();
                }
            };

            bool exit_app = false;
            auto on_quit = [&] {
                if (confirm_dialog("确认退出", "确定要退出程序吗？")) {
                    exit_app = true;
                    screen.Exit();
                }
            };

            auto menu = Menu(&display_files, &selected);
            auto buttons = Container::Horizontal({
                Button("编辑配置", on_edit),
                Button("删除配置", on_delete),
                Button("激活配置", on_activate),
                Button("新建配置", on_create),
                Button("退出应用", on_quit)
            });

            auto layout = Container::Vertical({ menu, buttons });

            auto renderer = Renderer(layout, [&] {
                return vbox({
                    text("配置管理器 - " + app_name) | bold | center,
                    separator(),
                    window(text("配置文件列表") | bold,
                           menu->Render() | frame | size(HEIGHT, LESS_THAN, 20)),
                    separator(),
                    buttons->Render() | center,
                    filler(),
                }) | border;
            });

            screen.Loop(renderer);

            if (exit_app) break;
        }
    }

}  // namespace ui
