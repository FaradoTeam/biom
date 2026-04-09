#include <iostream>
#include <regex>
#include <string>

#include <cpprest/http_msg.h>
#include <cpprest/json.h>

#include "common/log/log.h"

#include "auth_handler.h"

namespace farado
{
namespace server
{
namespace handlers
{

AuthHandler::AuthHandler(std::shared_ptr<AuthMiddleware> authMiddleware)
    : m_authMiddleware(authMiddleware)
{
    if (!m_authMiddleware)
    {
        LOG_ERROR << "Warning: AuthHandler initialized without AuthMiddleware";
    }
}

void AuthHandler::handleLogin(const web::http::http_request& request)
{
    try
    {
        // Извлекаем тело запроса
        request
            .extract_json()
            .then(
                [this, request](pplx::task<web::json::value> task)
                {
                    web::json::value jsonBody;
                    try
                    {
                        jsonBody = task.get();
                    }
                    catch (const std::exception& e)
                    {
                        web::http::http_response response(
                            web::http::status_codes::BadRequest
                        );
                        sendErrorResponse(
                            response,
                            400,
                            "Invalid JSON: " + std::string(e.what())
                        );
                        request.reply(response);
                        return;
                    }

                    const std::string login = jsonBody.has_field("login")
                        ? jsonBody.at("login").as_string()
                        : "";
                    const std::string password = jsonBody.has_field("password")
                        ? jsonBody.at("password").as_string()
                        : "";

                    // Валидация входных данных
                    if (login.empty() || password.empty())
                    {
                        web::http::http_response response(
                            web::http::status_codes::BadRequest
                        );
                        sendErrorResponse(
                            response,
                            400,
                            "Login and password are required"
                        );
                        request.reply(response);
                        return;
                    }

                    // Проверяем учетные данные
                    std::string userId = validateCredentials(login, password);
                    if (userId.empty())
                    {
                        web::http::http_response response(
                            web::http::status_codes::Unauthorized
                        );
                        sendErrorResponse(
                            response,
                            401,
                            "Invalid credentials"
                        );
                        request.reply(response);
                        return;
                    }

                    // Генерируем JWT токен
                    const std::string token = m_authMiddleware->generateToken(userId);

                    // Формируем ответ
                    web::json::value response;
                    response["access_token"] = web::json::value::string(token);
                    response["token_type"] = web::json::value::string("Bearer");
                    response["expires_in"] = web::json::value::number(3600);

                    request.reply(web::http::status_codes::OK, response);
                    LOG_INFO << "User " << userId << " logged in successfully";
                }
            )
            .wait();
    }
    catch (const std::exception& e)
    {
        web::http::http_response response(web::http::status_codes::BadRequest);
        sendErrorResponse(
            response,
            400,
            "Invalid request body: " + std::string(e.what())
        );
        request.reply(response);
    }
}

void AuthHandler::handleLogout(const web::http::http_request& request)
{
    // Извлекаем токен из заголовка
    auto authHeader = request.headers().find("Authorization");
    if (authHeader == request.headers().end())
    {
        LOG_ERROR << "No Authorization header in logout request";
        request.reply(web::http::status_codes::Unauthorized);
        return;
    }

    // Извлекаем Bearer токен
    std::regex bearerRegex(R"(^Bearer\s+([a-zA-Z0-9\-_\.]+)$)");
    std::smatch matches;

    if (std::regex_match(authHeader->second, matches, bearerRegex) && matches.size() > 1)
    {
        const std::string token = matches[1].str();
        LOG_INFO << "Logout: invalidating token";
        m_authMiddleware->invalidateToken(token);
        request.reply(web::http::status_codes::NoContent);
    }
    else
    {
        LOG_ERROR << "Invalid Authorization header format in logout";
        request.reply(web::http::status_codes::BadRequest);
    }

    LOG_INFO << "User logged out";
}

void AuthHandler::handleChangePassword(
    const web::http::http_request& request,
    const std::string& userId
)
{
    try
    {
        request
            .extract_json()
            .then(
                [this, request, userId](pplx::task<web::json::value> task)
                {
                    web::json::value jsonBody;
                    try
                    {
                        jsonBody = task.get();
                    }
                    catch (const std::exception& e)
                    {
                        web::http::http_response response(
                            web::http::status_codes::BadRequest
                        );
                        sendErrorResponse(
                            response,
                            400,
                            "Invalid JSON: " + std::string(e.what())
                        );
                        request.reply(response);
                        return;
                    }

                    const std::string oldPassword = jsonBody.has_field("oldPassword")
                        ? jsonBody.at("oldPassword").as_string()
                        : "";
                    const std::string newPassword = jsonBody.has_field("newPassword")
                        ? jsonBody.at("newPassword").as_string()
                        : "";

                    if (oldPassword.empty() || newPassword.empty())
                    {
                        web::http::http_response response(
                            web::http::status_codes::BadRequest
                        );
                        sendErrorResponse(
                            response,
                            400,
                            "Old password and new password are required"
                        );
                        request.reply(response);
                        return;
                    }

                    if (newPassword.length() < 8)
                    {
                        web::http::http_response response(
                            web::http::status_codes::BadRequest
                        );
                        sendErrorResponse(
                            response,
                            400,
                            "New password must be at least 8 characters long"
                        );
                        request.reply(response);
                        return;
                    }

                    // TODO: Здесь должна быть логика смены пароля в БД

                    request.reply(web::http::status_codes::NoContent);
                    LOG_INFO << "Password changed for user " << userId;
                }
            )
            .wait();
    }
    catch (const std::exception& e)
    {
        web::http::http_response response(web::http::status_codes::BadRequest);
        sendErrorResponse(
            response,
            400,
            "Invalid request body: " + std::string(e.what())
        );
        request.reply(response);
    }
}

std::string AuthHandler::validateCredentials(
    const std::string& login,
    const std::string& password
)
{
    // TODO: Реализовать проверку в БД
    // Временная заглушка для тестирования
    if (login == "admin" && password == "admin123")
    {
        return "1";
    }
    return "";
}

void AuthHandler::sendErrorResponse(
    web::http::http_response& response,
    int code,
    const std::string& message
)
{
    web::json::value error;
    error["code"] = web::json::value::number(code);
    error["message"] = web::json::value::string(message);
    response.set_body(error);
}

} // namespace handlers
} // namespace server
} // namespace farado
