#pragma once

#include <nlohmann/json.hpp>

namespace config {

    using json = nlohmann::ordered_json;

    // 根据 schema 递归生成默认配置模板
    json generate_default_config(const json& schema);

}  // namespace config
