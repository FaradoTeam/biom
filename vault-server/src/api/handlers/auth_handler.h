#pragma once

#include <memory>
#include <string>

#include <cpprest/http_msg.h>
#include <cpprest/json.h>

#include "../middleware/auth_middleware.h"

namespace farado
{
namespace server
{
namespace handlers
{

class AuthHandler final
{
public:
    explicit AuthHandler(std::shared_ptr<AuthMiddleware> authMiddleware);
    ~AuthHandler() = default;

    /**
     * Обрабатывает запрос на вход
     */
    void handleLogin(const web::http::http_request& request);

    /**
     * Обрабатывает запрос на выход
     */
    void handleLogout(const web::http::http_request& request);

    /**
     * Обрабатывает запрос на смену пароля
     */
    void handleChangePassword(
        const web::http::http_request& request,
        const std::string& userId
    );

private:
    std::string validateCredentials(
        const std::string& login,
        const std::string& password
    );
    void sendErrorResponse(
        web::http::http_response& response,
        int code,
        const std::string& message
    );

private:
    std::shared_ptr<AuthMiddleware> m_authMiddleware;
};

} // namespace handlers
} // namespace server
} // namespace farado
