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
#include <sstream>  // 新增

using namespace ftxui;
using json = config::json;
namespace fs = std::filesystem;

namespace ui {

// 新增：分割 json_pointer 路径字符串的辅助函数
static std::vector<std::string> split_path(const std::string& pointer) {
  std::vector<std::string> result;
  std::stringstream ss(pointer);
  std::string segment;
  while (std::getline(ss, segment, '/')) {
    if (!segment.empty()) {
      // JSON Pointer 解码： ~0 -> ~ , ~1 -> /
      std::string unescaped;
      for (size_t i = 0; i < segment.size(); ++i) {
        if (segment[i] == '~' && i + 1 < segment.size()) {
          if (segment[i + 1] == '0') unescaped += '~';
          else if (segment[i + 1] == '1') unescaped += '/';
          ++i;
        } else {
          unescaped += segment[i];
        }
      }
      result.push_back(unescaped);
    }
  }
  return result;
}

struct JsonPathEntry {
  std::vector<json::json_pointer> paths;
  std::vector<std::string> labels;
};

JsonPathEntry build_label_path_tree(const json& config, const json& schema, const std::string& base = "", json::json_pointer ptr = json::json_pointer("")) {
  JsonPathEntry result;

  if (!schema.contains("type")) return result;
  std::string type = schema["type"];

  if (type == "object" && schema.contains("properties")) {
    for (auto& [key, prop_schema] : schema["properties"].items()) {
      auto child_ptr = ptr / key;
      std::string label = base + key;
      result.labels.push_back(label);
      result.paths.push_back(child_ptr);

      auto child = build_label_path_tree(config, prop_schema, base + "  ", child_ptr);
      result.labels.insert(result.labels.end(), child.labels.begin(), child.labels.end());
      result.paths.insert(result.paths.end(), child.paths.begin(), child.paths.end());
    }
  } else if (type == "array" && schema.contains("items")) {
    const json& arr = config.contains(ptr) ? config[ptr] : json::array();
    for (int i = 0; i < arr.size(); ++i) {
      auto item_ptr = ptr / std::to_string(i);
      std::string label = base + "[" + std::to_string(i) + "]";
      result.labels.push_back(label);
      result.paths.push_back(item_ptr);

      auto child = build_label_path_tree(config, schema["items"], base + "  ", item_ptr);
      result.labels.insert(result.labels.end(), child.labels.begin(), child.labels.end());
      result.paths.insert(result.paths.end(), child.paths.begin(), child.paths.end());
    }
  }

  return result;
}

void edit_config(const std::string& path, const config::json& schema) {
  json config = config::load_config(path);

  std::string status_message;
  std::string description;
  std::string edit_buffer;

  auto screen = ScreenInteractive::Fullscreen();

  std::vector<std::string> menu_labels;
  std::vector<json::json_pointer> menu_paths;
  int selected = 0;

  auto update_menu_tree = [&] {
    auto entry = build_label_path_tree(config, schema);
    menu_labels = std::move(entry.labels);
    menu_paths = std::move(entry.paths);
  };

  auto select_path_by_index = [&] {
    if (selected >= 0 && selected < menu_paths.size()) {
      json::json_pointer ptr = menu_paths[selected];
      const json& val = config[ptr];

      // 获取 schema 路径
      const json* schema_ptr = &schema;
      auto tokens = split_path(ptr.to_string());
      for (const std::string& key : tokens) {
        if (schema_ptr->contains("type") && (*schema_ptr)["type"] == "array") {
          if (schema_ptr->contains("items"))
            schema_ptr = &(*schema_ptr)["items"];
        } else if (schema_ptr->contains("properties") && (*schema_ptr)["properties"].contains(key)) {
          schema_ptr = &(*schema_ptr)["properties"][key];
        }
      }

      if (schema_ptr->contains("description"))
        description = (*schema_ptr)["description"].get<std::string>();
      else
        description = "无描述";

      if (schema_ptr->contains("enum")) {
        edit_buffer = val.get<std::string>();
      } else if (schema_ptr->contains("type")) {
        std::string type = (*schema_ptr)["type"];
        if (type == "string") {
          edit_buffer = val.get<std::string>();
        } else {
          edit_buffer = val.dump();
        }
      } else {
        edit_buffer = val.dump();
      }
    }
  };

  update_menu_tree();
  if (!menu_paths.empty()) select_path_by_index();

  auto edit_input = Input(&edit_buffer, "编辑值");
  auto update_button = Button("更新", [&] {
    if (selected >= 0 && selected < menu_paths.size()) {
      try {
        json::json_pointer ptr = menu_paths[selected];
        const json* schema_ptr = &schema;
        auto tokens = split_path(ptr.to_string());
        for (const std::string& key : tokens) {
          if (schema_ptr->contains("type") && (*schema_ptr)["type"] == "array") {
            if (schema_ptr->contains("items"))
              schema_ptr = &(*schema_ptr)["items"];
          } else if (schema_ptr->contains("properties") && (*schema_ptr)["properties"].contains(key)) {
            schema_ptr = &(*schema_ptr)["properties"][key];
          }
        }

        json parsed;
        if (schema_ptr->contains("enum")) {
          parsed = edit_buffer;
        } else if (schema_ptr->contains("type")) {
          std::string type = (*schema_ptr)["type"];
          if (type == "string") {
            parsed = edit_buffer;
          } else {
            parsed = json::parse(edit_buffer);
          }
        } else {
          parsed = json::parse(edit_buffer);
        }

        config[ptr] = parsed;
        status_message = "更新成功";
        update_menu_tree();
      } catch (...) {
        status_message = "更新失败：无效 JSON 或类型不匹配";
      }
    }
  });

  MenuOption option;
  option.on_change = [&] {
    select_path_by_index();
  };

  auto menu = Menu(&menu_labels, &selected, option);
  auto right_column = Container::Vertical({edit_input, update_button});

  auto right_panel = Container::Vertical({
    Renderer([&] {
      return vbox({
        text("描述:") | bold,
        paragraph(description),
        separator(),
        hbox({text("当前值: "), text(edit_buffer)}),
        separator()
      });
    }),
    right_column
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
