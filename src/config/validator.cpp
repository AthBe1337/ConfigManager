#include "validator.hpp"
#include <nlohmann/json-schema.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>

namespace config {

    // schema loader：支持本地路径加载，后续可扩展为网络加载
    static void loader(const nlohmann::json_uri &uri, nlohmann::json &schema) {
        std::string filename = "./" + uri.path();
        std::ifstream lf(filename);
        if (!lf.good()) {
            throw std::invalid_argument("Could not open schema from URI: " + uri.url() +
                                        " (tried: " + filename + ")");
        }
        lf >> schema;
    }

    // 自定义错误处理器
    class custom_error_handler : public nlohmann::json_schema::basic_error_handler {
    public:
        void error(const nlohmann::json::json_pointer &ptr,
                   const nlohmann::json &instance,
                   const std::string &message) override {
            nlohmann::json_schema::basic_error_handler::error(ptr, instance, message);
            std::ostringstream oss;
            oss << "Validation error at " << ptr.to_string()
                << " (value: " << instance.dump() << "): " << message;
            errors.push_back(oss.str());
        }

        bool has_errors() const { return !errors.empty(); }
        std::string get_error_report() const {
            std::ostringstream oss;
            for (size_t i = 0; i < errors.size(); ++i) {
                oss << "[" << i + 1 << "] " << errors[i] << "\n";
            }
            return oss.str();
        }

    private:
        std::vector<std::string> errors;
    };

    // 验证接口
    void validate_config(const nlohmann::json &config, const nlohmann::json &schema) {
        if (schema.is_null() || !schema.is_object()) {
            throw std::invalid_argument("Invalid or empty schema provided");
        }

        nlohmann::json_schema::json_validator validator(loader, nlohmann::json_schema::default_string_format_check);
        validator.set_root_schema(schema);

        custom_error_handler err;
        validator.validate(config, err);

        if (err.has_errors()) {
            throw std::runtime_error("Config validation failed:\n" + err.get_error_report());
        }
    }

} // namespace config
