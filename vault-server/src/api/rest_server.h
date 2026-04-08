#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include "middleware/auth_middleware.h"

namespace farado
{
namespace server
{

class RestServer final
{
public:
    using RouteHandler = std::function<void(
        const web::http::http_request&,
        const std::string& userId
    )>;

    explicit RestServer(const std::string& host = "0.0.0.0", uint16_t port = 8080);
    ~RestServer();

    bool initialize();
    bool start();
    void stop();
    bool isRunning() const { return m_isRunning; }

    void setAuthMiddleware(std::shared_ptr<AuthMiddleware> middleware);

private:
    void registerRoutes();
    void setupListener();
    void handleRequest(web::http::http_request request);
    bool applyAuthMiddleware(
        const web::http::http_request& request,
        std::string& userId
    );

    void addRoute(
        const web::http::method& method,
        const std::string& path,
        RouteHandler handler,
        bool isPublic = false
    );

    bool matchRoute(
        const web::http::method& method,
        const std::string& uriPath,
        RouteHandler& handler,
        bool& isPublic,
        std::map<std::string, std::string>& params
    );

private:
    const std::string m_host;
    const uint16_t m_port;
    std::string m_baseUrl;

    std::unique_ptr<web::http::experimental::listener::http_listener> m_listener;
    std::shared_ptr<AuthMiddleware> m_authMiddleware;
    std::atomic<bool> m_isRunning { false };

    struct RouteInfo
    {
        web::http::method method;
        std::string pathPattern;
        RouteHandler handler;
        bool isPublic; // Флаг публичного маршрута (не требует аутентификации)
    };
    std::vector<RouteInfo> m_routes;
};

} // namespace server
} // namespace farado
