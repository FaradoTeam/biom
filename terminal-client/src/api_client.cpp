#include <cpprest/http_msg.h>
#include <cpprest/uri.h>

#include "common/log/log.h"

#include "api_client.h"

namespace api
{

Client::Client(const std::string& baseUrl)
    : m_client(utility::conversions::to_string_t(baseUrl))
{
    LOG_DEBUG << "API клиент создан для " << baseUrl;
}

std::future<AuthResult> Client::login(
    const std::string& login,
    const std::string& password
)
{
    LOG_DEBUG << "login: запрос для " << login;
    return std::async(
        std::launch::async,
        [this, login, password]() -> AuthResult
        {
            LOG_DEBUG << "Поток запроса: начало";
            web::json::value body;
            body[U("login")] = web::json::value::string(utility::conversions::to_string_t(login));
            body[U("password")] = web::json::value::string(utility::conversions::to_string_t(password));

            web::http::http_request request(web::http::methods::POST);
            request.set_body(body);
            request.set_request_uri(U("/api/v1/auth/login"));

            try
            {
                LOG_DEBUG << "Отправка запроса...";
                auto response = m_client.request(request).get();
                LOG_DEBUG << "Запрос выполнен, статус=" << response.status_code();
                if (response.status_code() == web::http::status_codes::OK)
                {
                    auto json = response.extract_json().get();
                    auto token = utility::conversions::to_utf8string(
                        json.at(U("access_token")).as_string()
                    );
                    LOG_INFO << "Успешный вход, токен получен";
                    return AuthResult { true, token, "" };
                }
                else
                {
                    auto json = response.extract_json().get();
                    auto msg = utility::conversions::to_utf8string(
                        json.at(U("message")).as_string()
                    );
                    LOG_WARN << "Ошибка входа: " << msg;
                    return AuthResult { false, "", msg };
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR << "Исключение в запросе: " << e.what();
                return AuthResult { false, "", e.what() };
            }
            catch (...)
            {
                LOG_ERROR << "Неизвестное исключение в запросе";
                return AuthResult { false, "", "Unknown error" };
            }
        }
    );
}

std::future<bool> Client::logout(const std::string& token)
{
    LOG_DEBUG << "logout: запрос на выход";
    return std::async(
        std::launch::async,
        [this, token]() -> bool
        {
            try
            {
                web::http::http_request request(web::http::methods::POST);
                request.set_request_uri(U("/api/v1/auth/logout"));
                request.headers().add(U("Authorization"), utility::conversions::to_string_t("Bearer " + token));

                auto response = m_client.request(request).get();
                LOG_DEBUG << "Запрос выхода выполнен, статус=" << response.status_code();
                if (response.status_code() == web::http::status_codes::NoContent)
                {
                    LOG_INFO << "Успешный выход";
                    return true;
                }
                else
                {
                    LOG_WARN << "Ошибка при выходе, статус " << response.status_code();
                    return false;
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR << "Исключение при выходе: " << e.what();
                return false;
            }
            catch (...)
            {
                LOG_ERROR << "Неизвестное исключение при выходе";
                return false;
            }
        }
    );
}

} // namespace api
