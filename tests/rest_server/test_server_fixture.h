#pragma once

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <boost/test/unit_test.hpp>
#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include "vault-server/src/api/rest_server.h"
#include "vault-server/src/api/middleware/auth_middleware.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;

namespace farado::server::test
{

// Тестовая конфигурация
const std::string TEST_HOST = "127.0.0.1";
const uint16_t TEST_PORT = 18080;
const std::string TEST_BASE_URL = "http://" + TEST_HOST + ":" + std::to_string(TEST_PORT);
const std::string TEST_SECRET_KEY = "your-very-long-secret-key-that-is-at-least-32-bytes-long!";

/**
 * @brief Вспомогательные функции для работы с JSON в тестах
 */
inline int64_t getJsonInt(const json::value& obj, const std::string& key)
{
    return obj.at(key).as_integer();
}

inline std::string getJsonString(const json::value& obj, const std::string& key)
{
    return obj.at(key).as_string();
}

inline bool hasJsonField(const json::value& obj, const std::string& key)
{
    return obj.has_field(key);
}

/**
 * @brief Вспомогательный класс для управления сервером в тестах
 */
struct TestServerFixture
{
    TestServerFixture()
        : m_server(std::make_shared<RestServer>(TEST_HOST, TEST_PORT))
    {
        auto authMiddleware = std::make_shared<AuthMiddleware>(TEST_SECRET_KEY);
        m_server->setAuthMiddleware(authMiddleware);
        m_server->initialize();
        m_server->start();
        
        // Даем серверу время на запуск
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    ~TestServerFixture()
    {
        if (m_server)
        {
            m_server->stop();
        }
    }
    
    std::shared_ptr<RestServer> getServer() const { return m_server; }
    
    std::string getTestToken() const
    {
        AuthMiddleware auth(TEST_SECRET_KEY);
        return auth.generateToken("test_user_123", 3600);
    }
    
    http_client createClient() const
    {
        return http_client { uri(TEST_BASE_URL) };
    }
    
    std::shared_ptr<RestServer> m_server;
};

} // farado::server::test
