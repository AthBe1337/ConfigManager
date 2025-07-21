#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace config {

    using json = nlohmann::ordered_json;

    json load_schema(const std::string& schema_path);

}  // namespace config