#include "edit.hpp"
#include "../utils/fs.hpp"
#include "../config.h"
#include "main_ui.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

using namespace ftxui;
using json = config::json;
namespace fs = std::filesystem;

namespace ui {

void edit_config(const std::string& path, const config::json& schema) {
  json config = config::load_config(path);

  std::string current_key;
  std::string status_message;
  std::string description;
  std::string edit_buffer;

  auto screen = ScreenInteractive::Fullscreen();

  std::vector<std::string> keys;
  for (auto& [key, prop] : schema["properties"].items()) {
    keys.push_back(key);
  }

  std::vector<std::string> menu_labels;
  int selected = 0;

  auto update_menu_labels = [&] {
    menu_labels.clear();
    for (const auto& key : keys) {
      menu_labels.push_back(key + " : " + config[key].dump());
    }
  };

  auto select_key_by_index = [&] {
    if (selected >= 0 && selected < keys.size()) {
      current_key = keys[selected];
      const json& value = config[current_key];
      const auto& prop_schema = schema["properties"][current_key];

      if (value.is_string()) {
        edit_buffer = value.get<std::string>();
      } else {
        edit_buffer = value.dump();
      }

      if (prop_schema.contains("description"))
        description = prop_schema["description"].get<std::string>();
      else
        description = "无描述";
    }
  };

  update_menu_labels();
  if (!keys.empty()) select_key_by_index();

  auto edit_input = Input(&edit_buffer, "编辑值");
  auto update_button = Button("更新", [&] {
    if (!current_key.empty()) {
      try {
        const auto& type = schema["properties"][current_key]["type"];
        json parsed;
        if (type == "string") {
          parsed = edit_buffer;
        } else if (type == "number" || type == "integer" || type == "boolean" || type == "array" || type == "object") {
          parsed = json::parse(edit_buffer);
        } else {
          throw std::runtime_error("不支持的数据类型");
        }

        config[current_key] = parsed;
        status_message = "更新成功";

        update_menu_labels();
        menu_labels[selected] = current_key + " : " + config[current_key].dump();
      } catch (...) {
        status_message = "解析失败，必须是合法 JSON 或合法字符串";
      }
    }
  });

  MenuOption option;
  option.on_change = [&] {
    select_key_by_index();
  };

  auto menu = Menu(&menu_labels, &selected, option);

  // 数组编辑器支持：增加/减少项
  Component array_tools = Renderer([] {
    return vbox({});
  });

  auto right_column = Container::Vertical({edit_input, update_button});
  auto right_panel = Container::Vertical({
    Renderer([&] {
      Elements elems = {
        text("描述:") | bold,
        paragraph(description),
        separator(),
        hbox({text("当前值: "), text(current_key.empty() ? "" : config[current_key].dump())}),
        separator(),
      };

      const auto& val = config[current_key];
      const auto& prop_schema = schema["properties"][current_key];

      if (val.is_array()) {
        // 显示数组子项
        int index = 0;
        for (const auto& item : val) {
          std::string prefix = "  -> [" + std::to_string(index++) + "] ";
          if (item.is_object()) {
            for (auto& [k, v] : item.items()) {
              elems.push_back(text(prefix + k + " : " + v.dump()));
            }
          } else {
            elems.push_back(text(prefix + item.dump()));
          }
        }
      }

      return vbox(elems);
    }),
    right_column,
    array_tools
  });

  auto layout = Container::Horizontal({menu, right_panel});

  auto on_save = [&] {
    try {
      config::save_config(path, config);
      status_message = "保存成功";
    } catch (const std::exception& e) {
      status_message = std::string("保存失败: ") + e.what();
    }
  };

  auto on_activate = [&] {
    try {
      config::validate_config(config, schema);
      config::set_active_config(path);
      status_message = "设为激活配置";
    } catch (const std::exception& e) {
      show_warning("校验失败", e.what());
    }
  };

  auto on_delete = [&] {
    if (confirm_dialog("确认删除", "是否删除该配置文件？")) {
      fs::remove(path);
      screen.Exit();
    }
  };

  auto buttons = Container::Horizontal({
    Button("保存", on_save),
    Button("设为激活", on_activate),
    Button("删除", on_delete),
    Button("返回", [&] { screen.Exit(); })
  });

  auto main_renderer = Renderer(Container::Vertical({layout, buttons}), [&] {
    return vbox({
      text("配置编辑器") | bold | center,
      separator(),
      hbox({
        vbox({
          text("设置项") | bold,
          separator(),
          menu->Render() | frame | size(HEIGHT, LESS_THAN, 20)
        }) | flex,
        separator(),
        vbox({
          text("详情") | bold,
          separator(),
          right_panel->Render()
        }) | size(WIDTH, LESS_THAN, 60)
      }),
      separator(),
      buttons->Render() | center,
      text(status_message) | color(Color::Yellow)
    }) | border;
  });

  screen.Loop(main_renderer);
}

}  // namespace ui