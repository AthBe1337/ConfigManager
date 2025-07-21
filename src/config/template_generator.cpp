#include "template_generator.hpp"

namespace config {

    json generate_default_config(const json& schema) {
        if (!schema.contains("type"))
            return nullptr;

        std::string type = schema["type"];

        if (type == "object") {
            json result = json::object();
            if (schema.contains("properties")) {
                for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it) {
                    const std::string& key = it.key();
                    const json& subschema = it.value();
                    result[key] = generate_default_config(subschema);
                }
            }
            return result;
        } else if (type == "array") {
            json arr = json::array();
            int count = 0;
            if (schema.contains("minItems"))
                count = schema["minItems"];
            for (int i = 0; i < count; ++i) {
                arr.push_back(generate_default_config(schema["items"]));
            }
            return arr;
        } else if (type == "string") {
            if (schema.contains("default"))
                return schema["default"];
            return "";
        } else if (type == "integer") {
            if (schema.contains("default"))
                return schema["default"];
            if (schema.contains("minimum"))
                return schema["minimum"];
            return 0;
        } else if (type == "boolean") {
            if (schema.contains("default"))
                return schema["default"];
            return false;
        }

        return nullptr; // 不支持或未定义类型
    }

}  // namespace config
