#include <regex>
#include <string>

#include <cpprest/http_msg.h>
#include <cpprest/json.h>

#include "common/log/log.h"

#include "auth_handler.h"

namespace server
{
namespace handlers
{

AuthHandler::AuthHandler(std::shared_ptr<AuthMiddleware> authMiddleware)
    : m_authMiddleware(authMiddleware)
{
    if (!m_authMiddleware)
    {
        LOG_WARN << "AuthHandler инициализирован без AuthMiddleware";
    }
}

void AuthHandler::handleLogin(const web::http::http_request& request)
{
    try
    {
        // Асинхронно извлекаем JSON из тела запроса
        request
            .extract_json()
            .then(
                [this, request](pplx::task<web::json::value> task)
                {
                    web::json::value jsonBody;
                    try
                    {
                        // Получаем результат парсинга JSON
                        jsonBody = task.get();
                    }
                    catch (const std::exception& e)
                    {
                        // Некорректный JSON в теле запроса
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

                    // Извлекаем логин и пароль из JSON
                    const std::string login = jsonBody.has_field("login")
                        ? jsonBody.at("login").as_string()
                        : "";
                    const std::string password = jsonBody.has_field("password")
                        ? jsonBody.at("password").as_string()
                        : "";

                    // Валидация: оба поля обязательны
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

                    // Проверяем учетные данные (в реальном коде — запрос к БД)
                    const std::string userId = validateCredentials(login, password);
                    if (userId.empty())
                    {
                        // Неверный логин или пароль
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

                    // Генерируем JWT-токен для успешно аутентифицированного пользователя
                    const std::string token = m_authMiddleware->generateToken(userId);

                    // Формируем успешный ответ с токеном
                    web::json::value response;
                    response["access_token"] = web::json::value::string(token);
                    response["token_type"] = web::json::value::string("Bearer");
                    // TODO : Вынести в настройки
                    // Время жизни токена в секундах
                    response["expires_in"] = web::json::value::number(3600);

                    request.reply(web::http::status_codes::OK, response);
                    LOG_INFO
                        << "Пользователь " << userId
                        << " успешно вошел в систему";
                }
            )
            .wait(); // Дожидаемся завершения асинхронной операции
    }
    catch (const std::exception& e)
    {
        // Ошибка при извлечении тела запроса
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
    // Ищем заголовок Authorization в запросе
    auto authHeader = request.headers().find("Authorization");
    if (authHeader == request.headers().end())
    {
        LOG_ERROR << "Нет заголовка авторизации в запросе на выход из системы";
        request.reply(web::http::status_codes::Unauthorized);
        return;
    }

    // Регулярное выражение для извлечения Bearer-токена
    // Формат: "Bearer <token>", где токен содержит буквы, цифры, дефис,
    // подчеркивание и точку
    std::regex bearerRegex(R"(^Bearer\s+([a-zA-Z0-9\-_\.]+)$)");
    std::smatch matches;

    if (std::regex_match(authHeader->second, matches, bearerRegex) && matches.size() > 1)
    {
        const std::string token = matches[1].str();
        // Добавляем токен в черный список
        m_authMiddleware->invalidateToken(token);
        request.reply(web::http::status_codes::NoContent);
    }
    else
    {
        // Неверный формат заголовка Authorization
        LOG_ERROR
            << "Недопустимый формат заголовка авторизации при выходе из системы";
        request.reply(web::http::status_codes::BadRequest);
    }

    LOG_INFO << "Пользователь вышел из системы";
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

                    // Извлекаем старый и новый пароли из запроса
                    const std::string oldPassword = jsonBody.has_field("oldPassword")
                        ? jsonBody.at("oldPassword").as_string()
                        : "";
                    const std::string newPassword = jsonBody.has_field("newPassword")
                        ? jsonBody.at("newPassword").as_string()
                        : "";

                    // Проверяем, что оба пароля присутствуют
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

                    // TODO : Вынести в настройки
                    // Проверяем минимальную длину нового пароля
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
                    // 1. Проверить, что старый пароль верный
                    // 2. Обновить пароль на новый (с хешированием)
                    // 3. Аннулировать все активные токены пользователя

                    request.reply(web::http::status_codes::NoContent);
                    LOG_INFO << "Пароль изменен для пользователя " << userId;
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
    // здесь должен быть запрос к БД и проверка хеша пароля
    if (login == "admin" && password == "admin123")
    {
        return "1"; // Возвращаем идентификатор пользователя
    }
    return ""; // Неверные учетные данные
}

void AuthHandler::sendErrorResponse(
    web::http::http_response& response,
    int code,
    const std::string& message
)
{
    // Формируем JSON с информацией об ошибке
    web::json::value error;
    error["code"] = web::json::value::number(code);
    error["message"] = web::json::value::string(message);
    response.set_body(error);
}

} // namespace handlers
} // namespace server
