#pragma once

#include <future>
#include <string>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

namespace api
{

struct AuthResult final
{
    bool success { false };
    std::string token;
    std::string errorMessage;
};

class Client final
{
public:
    explicit Client(const std::string& baseUrl);

    // Асинхронный вход
    std::future<AuthResult> login(const std::string& login, const std::string& password);

    // Асинхронный выход (аннулирование токена)
    std::future<bool> logout(const std::string& token);

private:
    web::http::client::http_client m_client;
};

} // namespace api