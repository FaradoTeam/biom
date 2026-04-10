cmake_minimum_required(VERSION 3.15.7)

project(farado_rest_server_tests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS unit_test_framework)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpprestsdk REQUIRED)
find_package(jwt-cpp REQUIRED)

set(API_SERVER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vault-server/src/api/rest_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vault-server/src/api/middleware/auth_middleware.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vault-server/src/api/handlers/auth_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vault-server/src/api/handlers/items_handler.cpp
)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rest_server.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_token_expiration.cpp
)

add_executable(
    ${PROJECT_NAME}
    ${API_SERVER_SRC}
    ${TEST_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vault-server/src
    ${Boost_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    boost::boost
    nlohmann_json::nlohmann_json
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    cpprestsdk::cpprest
    jwt-cpp::jwt-cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME RestServerTests COMMAND ${PROJECT_NAME})
