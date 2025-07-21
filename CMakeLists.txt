cmake_minimum_required(VERSION 3.14)
project(ConfigManager)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCE_FILES "src/*.h" "src/*.hpp" "src/*.cpp")

# 添加子模块路径
add_subdirectory(external/FTXUI)
add_subdirectory(external/json)
add_subdirectory(external/json-schema-validator)

# 添加可执行文件
add_executable(ConfigManager
        ${SOURCE_FILES}
)

# 链接依赖库
target_link_libraries(ConfigManager
        PRIVATE
        ftxui::screen
        ftxui::dom
        ftxui::component
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator::validator
)