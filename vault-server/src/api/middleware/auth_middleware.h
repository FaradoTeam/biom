#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace farado
{
namespace server
{

/**
 * @brief Структура, представляющая валидный JWT-токен.
 */
struct JWTToken
{
    std::string token;
    std::string userId;
    std::chrono::system_clock::time_point expiresAt;
};

/**
 * @brief Класс для аутентификации запросов через JWT токены.
 */
class AuthMiddleware
{
public:
    using Ptr = std::shared_ptr<AuthMiddleware>;

    explicit AuthMiddleware(const std::string& secretKey);
    ~AuthMiddleware() = default;

    /**
     * @brief Проверяет заголовок Authorization на наличие валидного Bearer-токена.
     * @param authHeader Значение заголовка Authorization.
     * @param userId Сюда будет записан user_id из токена в случае успеха.
     * @return true если токен присутствует, не аннулирован и прошёл верификацию.
     */
    bool validateRequest(const std::string& authHeader, std::string& userId);

    /**
     * @brief Генерирует новый JWT-токен для пользователя.
     */
    std::string generateToken(const std::string& userId, int expiresInSeconds = 3600);

    /**
     * @brief Аннулирует токен (добавляет в чёрный список).
     */
    void invalidateToken(const std::string& token);

    /**
     * @brief Проверяет, не был ли токен аннулирован.
     */
    bool isTokenInvalidated(const std::string& token);

private:
    std::string extractBearerToken(const std::string& authHeader);
    std::optional<JWTToken> verifyToken(const std::string& token);
    void cleanBlacklist();

private:
    std::string m_secretKey;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> m_blacklist;
    std::mutex m_blacklistMutex;
};

} // namespace server
} // namespace farado
