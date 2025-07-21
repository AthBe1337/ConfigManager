#include "init.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <filesystem>

using namespace ftxui;

namespace ui {

    std::string ask_input_blocking(const std::string& title, const std::string& placeholder) {
        std::string input;
        std::string result;
        std::string error_message;

        auto screen = ScreenInteractive::FitComponent();

        auto input_component = Input(&input, placeholder);

        auto button = Button("确定", [&] {
            if (input.empty()) {
                error_message = "输入不能为空！";
            } else {
                result = input;
                screen.Exit();
            }
        });

        auto layout = Container::Vertical({
            input_component,
            button,
        });

        auto renderer = Renderer(layout, [&] {
            Elements elements = {
                text(title) | bold,
                input_component->Render(),
                button->Render(),
            };

            if (!error_message.empty()) {
                elements.push_back(text(error_message) | color(Color::Red));
            }

            return vbox(elements) | border | center;
        });

        screen.Loop(renderer);
        return result;
    }

    std::string ask_app_name() {
        return ask_input_blocking("请输入应用名称", "");
    }

    std::string ask_schema_path() {
        std::string input;
        std::string result;
        std::string error_message;

        auto screen = ScreenInteractive::FitComponent();
        auto input_component = Input(&input, "");

        auto button = Button("确定", [&] {
            if (input.empty()) {
                error_message = "路径不能为空！";
            } else if (!std::filesystem::is_regular_file(input)) {
                error_message = "文件不存在或无效！";
            } else {
                result = input;
                screen.Exit();
            }
        });

        auto layout = Container::Vertical({
            input_component,
            button,
        });

        auto renderer = Renderer(layout, [&] {
            Elements elements = {
                text("未检测到 schema.json\n请输入 schema 路径") | bold,
                input_component->Render(),
                button->Render(),
            };
            if (!error_message.empty()) {
                elements.push_back(text(error_message) | color(Color::Red));
            }
            return vbox(elements) | border | center;
        });

        screen.Loop(renderer);
        return result;
    }

    void show_warning_message(const std::string& title, const std::string& message) {
        bool acknowledged = false;
        auto screen = ScreenInteractive::FitComponent();

        auto button = Button("确定", [&] {
            acknowledged = true;
            screen.Exit();
        });

        auto layout = Container::Vertical({button});

        auto renderer = Renderer(layout, [&] {
            return vbox({
                text(title) | bold,
                separator(),
                text(message),
                separator(),
                button->Render(),
            }) | border | center;
        });

        screen.Loop(renderer);
    }

}  // namespace ui
