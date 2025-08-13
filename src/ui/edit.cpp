#include "edit.hpp"
#include "ui_utils.hpp"
#include "../utils/fs.hpp"
#include "../config.h"
#include "main_ui.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <sstream>

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
    std::vector<std::string> values; // 新增：存储每个路径的当前值
  };

  JsonPathEntry build_label_path_tree(const json& config, const json& schema, const std::string& base = "", json::json_pointer ptr = json::json_pointer("")) {
    JsonPathEntry result;

    if (!schema.contains("type")) return result;
    std::string type = schema["type"];

    if (type == "object" && schema.contains("properties")) {
      for (auto& [key, prop_schema] : schema["properties"].items()) {
        auto child_ptr = ptr / key;
        std::string label = base + key;

        // 获取当前值（如果是基本类型）
        std::string value_str = "";
        if (config.contains(child_ptr)) {
          const json& val = config[child_ptr];
          if (prop_schema.contains("type")) {
            std::string prop_type = prop_schema["type"];
            if (prop_type != "object" && prop_type != "array") {
              if (prop_type == "boolean") {
                value_str = val.get<bool>() ? "true" : "false";
              } else if (prop_type == "string") {
                value_str = "\"" + val.get<std::string>() + "\"";
              } else {
                value_str = val.dump();
              }
            }
          } else {
            value_str = val.dump();
          }
        }

        // 如果值太长，截断
        if (value_str.length() > 15) {
          value_str = value_str.substr(0, 12) + "...";
        }

        result.labels.push_back(label);
        result.paths.push_back(child_ptr);
        result.values.push_back(value_str); // 存储值

        auto child = build_label_path_tree(config, prop_schema, base + "  ", child_ptr);
        result.labels.insert(result.labels.end(), child.labels.begin(), child.labels.end());
        result.paths.insert(result.paths.end(), child.paths.begin(), child.paths.end());
        result.values.insert(result.values.end(), child.values.begin(), child.values.end());
      }
    } else if (type == "array" && schema.contains("items")) {
      const json& arr = config.contains(ptr) ? config[ptr] : json::array();
      for (int i = 0; i < arr.size(); ++i) {
        auto item_ptr = ptr / std::to_string(i);
        std::string label = base + "[" + std::to_string(i) + "]";

        // 数组元素不显示值（只显示索引）
        result.labels.push_back(label);
        result.paths.push_back(item_ptr);
        result.values.push_back(""); // 空值

        auto child = build_label_path_tree(config, schema["items"], base + "  ", item_ptr);
        result.labels.insert(result.labels.end(), child.labels.begin(), child.labels.end());
        result.paths.insert(result.paths.end(), child.paths.begin(), child.paths.end());
        result.values.insert(result.values.end(), child.values.begin(), child.values.end());
      }
    }

    return result;
  }

  void edit_config(const std::string& path, const std::string& app_name, const config::json& schema) {
    json config = config::load_config(path);

    std::string status_message;
    std::string description;
    std::string edit_buffer;
    int enum_selected = 0;
    bool bool_value = false;

    // 持久的枚举选项向量
    std::vector<std::string> enum_options;

    auto screen = ScreenInteractive::Fullscreen();

    std::vector<std::string> menu_labels;
    std::vector<json::json_pointer> menu_paths;
    std::vector<std::string> menu_values;
    int selected = 0;

    auto update_menu_tree = [&] {
      auto entry = build_label_path_tree(config, schema);
      menu_labels = std::move(entry.labels);
      menu_paths = std::move(entry.paths);
      menu_values = std::move(entry.values);
    };

    // 存储当前选中的schema信息
    const json* current_schema_ptr = &schema;
    json::json_pointer current_parent_ptr;
    bool current_is_array = false;
    bool current_is_array_element = false;
    int current_min_items = 0;

    auto select_path_by_index = [&] {
      if (selected >= 0 && selected < menu_paths.size()) {
        json::json_pointer ptr = menu_paths[selected];
        const json& val = config[ptr];

        // 重置状态
        current_is_array = false;
        current_is_array_element = false;
        current_min_items = 0;

        // 获取当前指针的schema
        current_schema_ptr = &schema;
        auto tokens = split_path(ptr.to_string());
        for (const std::string& key : tokens) {
          if (current_schema_ptr->contains("type") && (*current_schema_ptr)["type"] == "array") {
            if (current_schema_ptr->contains("items"))
              current_schema_ptr = &(*current_schema_ptr)["items"];
          } else if (current_schema_ptr->contains("properties") && (*current_schema_ptr)["properties"].contains(key)) {
            current_schema_ptr = &(*current_schema_ptr)["properties"][key];
          }
        }

        // 检查是否是数组元素
        if (!ptr.empty()) {
          current_parent_ptr = ptr.parent_pointer();
          if (config.contains(current_parent_ptr) && config[current_parent_ptr].is_array()) {
            current_is_array_element = true;

            // 获取父数组的schema
            const json* parent_schema_ptr = &schema;
            auto parent_tokens = split_path(current_parent_ptr.to_string());
            for (const std::string& key : parent_tokens) {
              if (parent_schema_ptr->contains("type") && (*parent_schema_ptr)["type"] == "array") {
                if (parent_schema_ptr->contains("items"))
                  parent_schema_ptr = &(*parent_schema_ptr)["items"];
              } else if (parent_schema_ptr->contains("properties") && (*parent_schema_ptr)["properties"].contains(key)) {
                parent_schema_ptr = &(*parent_schema_ptr)["properties"][key];
              }
            }

            // 获取minItems约束
            if (parent_schema_ptr->contains("minItems")) {
              current_min_items = (*parent_schema_ptr)["minItems"].get<int>();
            }
          }
        }

        // 检查当前项是否是数组
        if (current_schema_ptr->contains("type") && (*current_schema_ptr)["type"] == "array") {
          current_is_array = true;
          if (current_schema_ptr->contains("minItems")) {
            current_min_items = (*current_schema_ptr)["minItems"].get<int>();
          }
        }

        if (current_schema_ptr->contains("description"))
          description = (*current_schema_ptr)["description"].get<std::string>();
        else
          description = "无描述";

        if (current_schema_ptr->contains("type") && (*current_schema_ptr)["type"] == "boolean") {
          bool_value = val.get<bool>();
          edit_buffer = "";
        }
        else if (current_schema_ptr->contains("enum")) {
          const auto& enum_vals = (*current_schema_ptr)["enum"];
          std::string current_val = val.get<std::string>();

          // 更新枚举选项
          enum_options.clear();
          for (const auto& option : enum_vals) {
            enum_options.push_back(option.get<std::string>());
          }

          // 设置当前选中项
          for (int i = 0; i < enum_options.size(); i++) {
            if (enum_options[i] == current_val) {
              enum_selected = i;
              break;
            }
          }
          edit_buffer = "";
        }
        else if (current_schema_ptr->contains("type")) {
          std::string type = (*current_schema_ptr)["type"];
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

    // 创建带值的菜单项
    std::vector<std::string> menu_items;
    auto update_menu_items = [&] {
      menu_items.clear();
      for (size_t i = 0; i < menu_labels.size(); i++) {
        if (!menu_values[i].empty()) {
          menu_items.push_back(menu_labels[i] + ": " + menu_values[i]);
        } else {
          menu_items.push_back(menu_labels[i]);
        }
      }
    };

    // 初始化菜单项
    update_menu_items();

    // 更新按钮
    auto update_button = Button("更新", [&] {
      if (selected >= 0 && selected < menu_paths.size()) {
        try {
          json::json_pointer ptr = menu_paths[selected];
          json parsed;

          if (current_schema_ptr->contains("type") && (*current_schema_ptr)["type"] == "boolean") {
            parsed = bool_value;
          }
          else if (current_schema_ptr->contains("enum")) {
            parsed = enum_options[enum_selected];
          }
          else if (current_schema_ptr->contains("type")) {
            std::string type = (*current_schema_ptr)["type"];
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

          // 更新菜单树和菜单项
          update_menu_tree();
          update_menu_items();
        } catch (...) {
          status_message = "更新失败：无效 JSON 或类型不匹配";
        }
      }
    });

    // 添加数组项按钮
    auto add_button = Button("添加新项", [&] {
      if (selected >= 0 && selected < menu_paths.size() && current_is_array) {
        try {
          json::json_pointer array_ptr = menu_paths[selected];

          // 创建新项的默认值
          if (current_schema_ptr->contains("items")) {
            json new_item = config::generate_default_config((*current_schema_ptr)["items"]);
            config[array_ptr].push_back(new_item);

            status_message = "已添加新项";

            // 更新菜单树和菜单项
            update_menu_tree();
            update_menu_items();

            // 选中新添加的项
            int new_index = config[array_ptr].size() - 1;
            json::json_pointer new_ptr = array_ptr / std::to_string(new_index);
            for (int i = 0; i < menu_paths.size(); i++) {
              if (menu_paths[i] == new_ptr) {
                selected = i;
                select_path_by_index();
                break;
              }
            }
          }
        } catch (...) {
          status_message = "添加失败";
        }
      }
    });

    // 删除数组项按钮
    auto delete_button = Button("删除此项", [&] {
      if (selected >= 0 && selected < menu_paths.size() && current_is_array_element) {
        try {
          json::json_pointer element_ptr = menu_paths[selected];
          json::json_pointer parent_ptr = element_ptr.parent_pointer();

          // 检查minItems约束
          int current_size = config[parent_ptr].size();
          if (current_min_items > 0 && current_size <= current_min_items) {
            status_message = "无法删除：数组元素数量不能小于minItems(" + std::to_string(current_min_items) + ")";
            return;
          }

          // 确认对话框
          if (confirm_dialog("确认删除", "是否删除该项？")) {
            // 获取索引
            std::string index_str = element_ptr.back();
            int index = std::stoi(index_str);

            // 删除元素
            json& arr = config[parent_ptr];
            arr.erase(arr.begin() + index);

            status_message = "已删除项";

            // 更新菜单树和菜单项
            update_menu_tree();
            update_menu_items();

            // 选中父数组
            for (int i = 0; i < menu_paths.size(); i++) {
              if (menu_paths[i] == parent_ptr) {
                selected = i;
                select_path_by_index();
                break;
              }
            }
          }
        } catch (...) {
          status_message = "删除失败";
        }
      }
    });

    MenuOption option;
    option.on_change = [&] {
      select_path_by_index();
    };

    // 使用标准Menu组件
    auto menu = Menu(&menu_items, &selected, option);

    // 固定左右面板大小
    int left_panel_width = 50; // 左侧面板宽度
    int right_panel_width = 60; // 右侧面板宽度

    // 右侧面板组件
    Component description_display = Renderer([&] {
      int max_width = std::max(20, right_panel_width);
      auto lines = ui::make_wrapped_text(description, max_width);

      return vbox({
        text("描述:") | bold,
        vbox(lines)
          | size(WIDTH, EQUAL, max_width)   // 确保宽度和 wrap 时一致
          | size(HEIGHT, EQUAL, 7)
          | frame
          | vscroll_indicator
      });
    });


    Component current_value_display = Renderer([&] {
      std::string current_value;
      if (selected >= 0 && selected < menu_paths.size()) {
        json::json_pointer ptr = menu_paths[selected];
        const json& val = config[ptr];

        if (current_schema_ptr->contains("type") && (*current_schema_ptr)["type"] == "boolean") {
          current_value = bool_value ? "true" : "false";
        } else if (current_schema_ptr->contains("enum")) {
          current_value = enum_options.empty() ? "" : enum_options[enum_selected];
        } else {
          current_value = val.dump();
        }
      }
      return hbox({text("当前值: "), text(current_value)});
    });

    Component separator_renderer = Renderer([] { return separator(); });

    // 动态编辑器组件
    Component editor_component = Input(&edit_buffer, "编辑值");

    // 右侧面板容器
    auto right_panel = Container::Vertical({});
    auto array_buttons = Container::Horizontal({});

    // 更新右侧面板
    auto update_right_panel = [&] {
      right_panel->DetachAllChildren();
      array_buttons->DetachAllChildren();

      // 添加固定组件
      right_panel->Add(description_display);
      if (!current_is_array && (!current_schema_ptr->contains("type") || (*current_schema_ptr)["type"] != "object")) {
        right_panel->Add(current_value_display);
      }
      right_panel->Add(separator_renderer);

      // 判断当前项是否为数组或对象，只有当前项既不是数组也不是对象时才显示编辑相关组件
      if (!current_is_array && (!current_schema_ptr->contains("type") || (*current_schema_ptr)["type"] != "object")) {
        if (selected >= 0 && selected < menu_paths.size()) {
          // 布尔类型 - 显示复选框
          if (current_schema_ptr->contains("type") && (*current_schema_ptr)["type"] == "boolean") {
            editor_component = Checkbox("启用", &bool_value);
          }
          // 枚举类型 - 显示切换按钮
          else if (current_schema_ptr->contains("enum")) {
            // 使用持久的 enum_options 向量
            editor_component = Radiobox(&enum_options, &enum_selected);
          }
          // 其他类型 - 显示文本输入框
          else {
            editor_component = Input(&edit_buffer, "编辑值");
          }
        } else {
          editor_component = Input(&edit_buffer, "编辑值");
        }

        // 添加编辑器和更新按钮
        right_panel->Add(editor_component);
        right_panel->Add(update_button);
      }
      // 添加数组操作按钮
      if (current_is_array) {
        array_buttons->Add(add_button);
      }
      if (current_is_array_element) {
        array_buttons->Add(delete_button);
      }

      if (array_buttons->ChildCount() > 0) {
        right_panel->Add(array_buttons);
      }
    };

    // 初始化右侧面板
    update_right_panel();

    auto layout = Container::Horizontal({
      // 左侧面板（菜单）
      menu | size(WIDTH, EQUAL, left_panel_width),
      // 右侧面板
      right_panel | size(WIDTH, EQUAL, right_panel_width)
    });

    auto on_save = [&] {
      try {
        config::save_config(path, config);
        status_message = "保存成功";
      } catch (const std::exception& e) {
        status_message = std::string("保存失败: ") + e.what();
      }
    };

    auto on_activate = [&] {
      // 先保存配置
      try {
        config::save_config(path, config);
        status_message = "保存成功";
      } catch (const std::exception& e) {
        status_message = std::string("保存失败: ") + e.what();
        show_warning("保存失败, 未激活: ", e.what());
        return;
      }

      // 保存成功后激活配置
      try {
        config::validate_config(config, schema);
        config::set_active_config(path);
        status_message = "已设为激活配置";
      } catch (const std::exception& e) {
        show_warning("校验失败: ", e.what());
      }
    };

    auto on_delete = [&] {
      if (confirm_dialog("确认删除", "是否删除该配置文件？")) {
        std::string active_config;
        try {
          active_config = config::get_active_config_path();
        } catch (const std::exception& e) {
          // 忽略
        }
        if (!active_config.empty() && fs::exists(active_config) && fs::equivalent(path, active_config)) {
          config::remove_active_config_link();
        }
        fs::remove(path);
        screen.Exit();
        run_main_ui(app_name, schema);
      }
    };

    auto buttons = Container::Horizontal({
      Button("保存配置", on_save),
      Button("激活配置", on_activate),
      Button("删除配置", on_delete),
      Button("返回", [&] {
        screen.Exit();
        run_main_ui(app_name, schema);
      })
    });

    // 修复：使用正确的容器结构
    auto main_container = Container::Vertical({
      layout,
      buttons
    });

    auto main_renderer = Renderer(main_container, [&] {
      // 当选中项变化时更新右侧面板
      static int last_selected = -1;
      if (selected != last_selected) {
          update_right_panel();
          last_selected = selected;
      }

      return vbox({
        text("配置编辑器") | bold | center,
        separator(),
        hbox({
          // 左侧面板
          vbox({
            text("设置项") | bold,
            separator(),
            menu->Render()
              | vscroll_indicator
              | frame
              | size(HEIGHT, LESS_THAN, 20)
          }) | border
            | size(WIDTH, EQUAL, left_panel_width + 4),

          // 右侧面板
          vbox({
            text("详情") | bold,
            separator(),
            right_panel->Render()
              | size(WIDTH, EQUAL, right_panel_width) // 限制实际内容宽度
          }) | border
            | size(WIDTH, EQUAL, right_panel_width + 4)
        }),
        separator(),
        buttons->Render() | center,
        text(status_message) | color(Color::Yellow)
      }) | border;
    });

    screen.Loop(main_renderer);
  }

}  // namespace ui