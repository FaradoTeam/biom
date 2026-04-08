#include <regex>

#include <jwt-cpp/jwt.h>

#include "common/log/log.h"

#include "auth_middleware.h"

namespace farado
{
namespace server
{

AuthMiddleware::AuthMiddleware(const std::string& secretKey)
    : m_secretKey(secretKey)
{
    if (m_secretKey.empty())
    {
        LOG_ERROR
            << "Warning: AuthMiddleware initialized with empty secret key";
    }
}

bool AuthMiddleware::validateRequest(const std::string& authHeader, std::string& userId)
{
    // 1. Проверяем наличие заголовка Authorization
    if (authHeader.empty())
    {
        LOG_ERROR << "Missing Authorization header";
        return false;
    }

    // 2. Извлекаем Bearer-токен
    std::string token = extractBearerToken(authHeader);
    if (token.empty())
    {
        LOG_ERROR << "Invalid Authorization header format";
        return false;
    }

    // 3. Проверяем, не аннулирован ли токен
    if (isTokenInvalidated(token))
    {
        LOG_ERROR << "Token has been invalidated";
        return false;
    }

    // 4. Верифицируем подпись, срок действия и issuer
    auto jwtToken = verifyToken(token);
    if (!jwtToken.has_value())
    {
        LOG_ERROR << "Invalid or expired token";
        return false;
    }

    // 5. Всё успешно — возвращаем userId из токена
    userId = jwtToken->userId;
    return true;
}

std::string AuthMiddleware::generateToken(const std::string& userId, int expiresInSeconds)
{
    const auto now = std::chrono::system_clock::now();
    const auto expiresAt = now + std::chrono::seconds(expiresInSeconds);

    return jwt::create()
        .set_type("JWT")
        .set_issuer("farado-api")
        .set_payload_claim("user_id", jwt::claim(userId))
        .set_issued_at(now)
        .set_expires_at(expiresAt)
        .sign(jwt::algorithm::hs256{m_secretKey});
}

std::optional<JWTToken> AuthMiddleware::verifyToken(const std::string& token)
{
    try
    {
        auto decoded = jwt::decode(token);

        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{m_secretKey})
                            .with_issuer("farado-api");

        verifier.verify(decoded);

        JWTToken result;
        result.token = token;
        result.userId = decoded.get_payload_claim("user_id").as_string();
        result.expiresAt = std::chrono::system_clock::from_time_t(
            decoded.get_expires_at().time_since_epoch().count()
        );

        return result;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Token verification failed: " << e.what();
    }
    return std::nullopt;
}

void AuthMiddleware::invalidateToken(const std::string& token)
{
    auto jwtToken = verifyToken(token);
    if (!jwtToken.has_value())
    {
        LOG_ERROR << "Cannot invalidate invalid token";
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_blacklistMutex);
        m_blacklist[token] = jwtToken->expiresAt;
    }

    auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(m_blacklistMutex);
    for (auto it = m_blacklist.begin(); it != m_blacklist.end();)
    {
        if (it->second < now)
        {
            it = m_blacklist.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool AuthMiddleware::isTokenInvalidated(const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_blacklistMutex);

    auto it = m_blacklist.find(token);
    if (it == m_blacklist.end())
    {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    if (it->second < now)
    {
        m_blacklist.erase(it);
        return false;
    }

    return true;
}

std::string AuthMiddleware::extractBearerToken(const std::string& authHeader)
{
    std::regex bearerRegex(R"(^Bearer\s+([a-zA-Z0-9\-_\.]+)$)");
    std::smatch matches;

    if (std::regex_match(authHeader, matches, bearerRegex) && matches.size() > 1)
    {
        return matches[1].str();
    }

    return "";
}

} // namespace server
} // namespace farado
