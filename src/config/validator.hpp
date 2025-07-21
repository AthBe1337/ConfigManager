#pragma once

#include <nlohmann/json.hpp>

namespace config {
    
    // 验证配置json是否符合schema，验证失败会抛异常
    void validate_config(const nlohmann::json& config, const nlohmann::json& schema);

}
