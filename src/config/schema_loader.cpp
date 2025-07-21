#include "schema_loader.hpp"
#include <fstream>
#include <stdexcept>

namespace config {

    json load_schema(const std::string& schema_path) {
        std::ifstream ifs(schema_path);
        if (!ifs.is_open()) {
            throw std::runtime_error("Failed to open schema file: " + schema_path);
        }

        json schema_json;
        try {
            ifs >> schema_json;
        } catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("Failed to parse schema JSON: " + std::string(e.what()));
        }

        return schema_json;
    }

}  // namespace config