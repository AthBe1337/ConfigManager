#include "validator.hpp"
#include <nlohmann/json-schema.hpp>
#include <fstream>
//#include <iostream>

namespace config {

    // schema loader，用于加载可能的远程引用schema，这里示例是本地当前目录加载
    static void loader(const nlohmann::json_uri &uri, nlohmann::json &schema) {
        std::string filename = "./" + uri.path();
        std::ifstream lf(filename);
        if (!lf.good())
            throw std::invalid_argument("could not open " + uri.url() + " tried with " + filename);
        lf >> schema;
    }

    // 自定义错误处理器，继承自库的basic_error_handler，打印详细错误信息
    class custom_error_handler : public nlohmann::json_schema::basic_error_handler {
    public:
        void error(const nlohmann::json::json_pointer &ptr, const nlohmann::json &instance,
                   const std::string &message) override {
            nlohmann::json_schema::basic_error_handler::error(ptr, instance, message);
            //std::cerr << "Validation error at '" << ptr << "' - instance: '" << instance << "' : " << message << "\n";
        }
    };

    // 验证接口，传入待验证json和schema
    void validate_config(const nlohmann::json &config, const nlohmann::json &schema) {
        // 创建验证器，传入loader和默认的格式检查器
        nlohmann::json_schema::json_validator validator(loader, nlohmann::json_schema::default_string_format_check);

        // 设置根schema，准备验证
        validator.set_root_schema(schema);

        // 创建错误处理器
        custom_error_handler err;

        // 进行验证
        validator.validate(config, err);

        // 抛出异常或者返回错误可以根据需求调整
        if (err) {
            throw std::runtime_error("Config validation failed");
        }
    }

}  // namespace config