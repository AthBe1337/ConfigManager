#include "edit.hpp"
#include "main_ui.hpp"
#include "../utils/fs.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <ctime>

using namespace ftxui;

namespace ui {

    void edit_config(const std::string& filepath, const config::json& schema) {
        config::json config_data = config::load_config(filepath);
        auto screen = ScreenInteractive::Fullscreen();

        // 用于存储数值输入的临时字符串
        std::unordered_map<std::string, std::shared_ptr<std::string>> number_inputs;

        std::function<Component(const std::string&, config::json&, const config::json&)> render_json_edit;

        render_json_edit = [&](const std::string& key, config::json& value, const config::json& subschema) -> Component {
            if (subschema["type"] == "string") {
                return Input(&value.get_ref<std::string&>(), key);
            }
            else if (subschema["type"] == "integer") {
                // 整数类型处理
                if (!number_inputs.count(key)) {
                    number_inputs[key] = std::make_shared<std::string>(std::to_string(value.get<int>()));
                }

                auto input = Input(number_inputs[key].get(), key);

                return Container::Vertical({
                    input
                }) | CatchEvent([str = number_inputs[key], &value](Event event) {
                    try {
                        value = std::stoi(*str);
                    } catch (...) {
                        // 转换失败时保持原值
                    }
                    return false;
                });
            }
            else if (subschema["type"] == "number") {
                // 浮点数类型处理
                if (!number_inputs.count(key)) {
                    number_inputs[key] = std::make_shared<std::string>(std::to_string(value.get<double>()));
                }

                auto input = Input(number_inputs[key].get(), key);

                return Container::Vertical({
                    input
                }) | CatchEvent([str = number_inputs[key], &value](Event event) {
                    try {
                        value = std::stod(*str);
                    } catch (...) {
                        // 转换失败时保持原值
                    }
                    return false;
                });
            }
            else if (subschema["type"] == "boolean") {
                return Checkbox(key, &value.get_ref<bool&>());
            }
            else if (subschema["type"] == "array") {
                int min_items = subschema.value("minItems", 0);
                auto container = Container::Vertical({});

                for (int i = 0; i < value.size(); ++i) {
                    auto element_container = Container::Horizontal({});
                    element_container->Add(
                        render_json_edit(key + "[" + std::to_string(i) + "]", value[i], subschema["items"])
                    );
                    element_container->Add(
                        Button("删除", [&, i] {
                            if (value.size() > min_items) {
                                value.erase(value.begin() + i);
                            } else {
                                show_warning("删除失败", "不能少于最小项数 " + std::to_string(min_items));
                            }
                        })
                    );
                    container->Add(element_container);
                }

                container->Add(
                    Button("添加", [&] {
                        value.push_back(config::generate_default_config(subschema["items"]));
                    })
                );

                return container;
            }
            else if (subschema["type"] == "object") {
                auto container = Container::Vertical({});
                for (auto& [k, subs] : subschema["properties"].items()) {
                    if (value.contains(k)) {
                        container->Add(render_json_edit(k, value[k], subs));
                    }
                }
                return container;
            }
            return Renderer([] { return text("未知类型"); });
        };

        auto root_component = render_json_edit("", config_data, schema);

        bool exit_editor = false;

        auto on_save = [&] {
            try {
                config::save_config(filepath, config_data);
                show_warning("保存成功", "配置已保存");
            } catch (const std::exception& e) {
                show_warning("保存失败", e.what());
            }
        };

        auto on_activate = [&] {
            try {
                config::validate_config(config_data, schema);
                config::save_config(filepath, config_data);
                config::set_active_config(filepath);
                show_warning("激活成功", "配置已设为激活");
            } catch (const std::exception& e) {
                show_warning("校验失败", e.what());
            }
        };

        auto on_delete = [&] {
            if (confirm_dialog("确认删除", "确定删除配置？")) {
                fs::remove(filepath);
                exit_editor = true;
                screen.Exit();
            }
        };

        auto on_quit = [&] {
            if (confirm_dialog("确认退出", "放弃修改并退出？")) {
                exit_editor = true;
                screen.Exit();
            }
        };

        auto buttons = Container::Horizontal({
            Button("保存", on_save),
            Button("激活", on_activate),
            Button("删除", on_delete),
            Button("退出", on_quit)
        });

        auto layout = Container::Vertical({root_component, buttons});

        auto renderer = Renderer(layout, [&] {
            return vbox({
                text("编辑配置文件 - " + filepath) | bold | center,
                separator(),
                layout->Render() | frame | size(HEIGHT, GREATER_THAN, 20),
            }) | border;
        });

        screen.Loop(renderer);

        if (exit_editor) {
            return;
        }
    }


}  // namespace ui
