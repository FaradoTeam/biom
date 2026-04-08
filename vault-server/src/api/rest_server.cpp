#include <chrono>
#include <memory>
#include <regex>

#include <cpprest/http_msg.h>
#include <cpprest/uri.h>

#include "common/log/log.h"

#include "handlers/auth_handler.h"
#include "handlers/items_handler.h"
#include "rest_server.h"

namespace farado
{
namespace server
{

RestServer::RestServer(const std::string& host, uint16_t port)
    : m_host(host)
    , m_port(port)
    , m_baseUrl("http://" + host + ":" + std::to_string(port))
    , m_isRunning(false)
{
    LOG_INFO
        << "RestServer created with host=" << m_host << ", port=" << m_port;
}

RestServer::~RestServer()
{
    stop();
}

bool RestServer::initialize()
{
    LOG_INFO << "Initializing REST server...";

    registerRoutes();
    setupListener();

    LOG_INFO << "REST server initialized successfully";
    return true;
}

void RestServer::setupListener()
{
    const web::http::uri address = web::http::uri(m_baseUrl);
    m_listener = std::make_unique<web::http::experimental::listener::http_listener>(
        address
    );

    m_listener->support(
        [this](web::http::http_request request)
        {
            try
            {
                handleRequest(std::move(request));
            }
            catch (const std::exception& e)
            {
                LOG_ERROR << "Request handling error: " << e.what();
                web::json::value error;
                error["code"] = web::json::value::number(500);
                error["message"] = web::json::value::string(
                    "Internal server error: " + std::string(e.what())
                );
                request.reply(web::http::status_codes::InternalError, error);
            }
        }
    );
}

void RestServer::registerRoutes()
{
    auto itemsHandler = std::make_shared<handlers::ItemsHandler>();
    auto authHandler = std::make_shared<handlers::AuthHandler>(m_authMiddleware);

    // Публичные маршруты (без аутентификации)
    addRoute(
        web::http::methods::POST,
        "/auth/login",
        [authHandler](
            const web::http::http_request& request,
            const std::string& /*userId*/
        )
        {
            authHandler->handleLogin(request);
        },
        true // isPublic
    );

    addRoute(
        web::http::methods::POST,
        "/auth/logout",
        [authHandler](
            const web::http::http_request& request,
            const std::string& /*userId*/
        )
        {
            authHandler->handleLogout(request);
        },
        true // isPublic
    );

    // Защищенные маршруты (требуют аутентификации)
    addRoute(
        web::http::methods::GET,
        "/api/items",
        [itemsHandler](
            const web::http::http_request& request,
            const std::string& userId
        )
        {
            itemsHandler->handleGetItems(request, userId);
        },
        false // requires auth
    );

    addRoute(
        web::http::methods::POST,
        "/api/items",
        [itemsHandler](
            const web::http::http_request& request,
            const std::string& userId
        )
        {
            itemsHandler->handleCreateItem(request, userId);
        },
        false
    );

    addRoute(
        web::http::methods::GET,
        R"(/api/items/(\d+))",
        [itemsHandler](
            const web::http::http_request& request,
            const std::string& userId
        )
        {
            itemsHandler->handleGetItem(request, userId);
        },
        false
    );

    addRoute(
        web::http::methods::PUT,
        R"(/api/items/(\d+))",
        [itemsHandler](
            const web::http::http_request& request,
            const std::string& userId
        )
        {
            itemsHandler->handleUpdateItem(request, userId);
        },
        false
    );

    addRoute(
        web::http::methods::DEL,
        R"(/api/items/(\d+))",
        [itemsHandler](
            const web::http::http_request& request,
            const std::string& userId
        )
        {
            itemsHandler->handleDeleteItem(request, userId);
        },
        false
    );

    // Health check - публичный
    addRoute(
        web::http::methods::GET,
        "/health",
        [](
            const web::http::http_request& request,
            const std::string& /*userId*/
        )
        {
            web::json::value response;
            response["status"] = web::json::value::string("ok");
            response["timestamp"] = web::json::value::number(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()
            );
            request.reply(web::http::status_codes::OK, response);
        },
        true // isPublic
    );

    LOG_INFO << "Routes registered successfully, total: " << m_routes.size();
}

void RestServer::addRoute(
    const web::http::method& method,
    const std::string& path,
    RouteHandler handler,
    bool isPublic
)
{
    RouteInfo route;
    route.method = method;
    route.pathPattern = path;
    route.handler = handler;
    route.isPublic = isPublic;
    m_routes.push_back(route);
}

bool RestServer::matchRoute(
    const web::http::method& method,
    const std::string& uriPath,
    RouteHandler& handler,
    bool& isPublic,
    std::map<std::string, std::string>& params
)
{
    for (const auto& route : m_routes)
    {
        if (route.method != method)
            continue;

        std::regex pattern(route.pathPattern);
        std::smatch matches;

        if (std::regex_match(uriPath, matches, pattern))
        {
            handler = route.handler;
            isPublic = route.isPublic;

            // Извлекаем параметры из пути (например, ID)
            if (matches.size() > 1)
            {
                params["id"] = matches[1].str();
            }
            return true;
        }
    }
    return false;
}

void RestServer::handleRequest(web::http::http_request request)
{
    const std::string path = web::uri::decode(request.relative_uri().path());
    const auto method = request.method();

    LOG_INFO << "Incoming request: " << method << " " << path;

    RouteHandler handler;
    bool isPublic = false;
    std::map<std::string, std::string> pathParams;

    if (!matchRoute(method, path, handler, isPublic, pathParams))
    {
        web::json::value error;
        error["code"] = web::json::value::number(404);
        error["message"] = web::json::value::string("Not found");
        request.reply(web::http::status_codes::NotFound, error);
        return;
    }

    // Аутентификация только для защищенных маршрутов
    if (!isPublic)
    {
        std::string userId;
        if (!applyAuthMiddleware(request, userId))
        {
            return; // Ответ уже отправлен в applyAuthMiddleware
        }
        handler(request, userId);
    }
    else
    {
        handler(request, "");
    }
}

bool RestServer::applyAuthMiddleware(
    const web::http::http_request& request,
    std::string& userId
)
{
    if (!m_authMiddleware)
    {
        LOG_ERROR << "Auth middleware not set";
        web::json::value error;
        error["code"] = web::json::value::number(500);
        error["message"] = web::json::value::string(
            "Internal server error: auth middleware not configured"
        );
        request.reply(web::http::status_codes::InternalError, error);
        return false;
    }

    auto authHeader = request.headers().find("Authorization");
    if (authHeader == request.headers().end())
    {
        web::json::value error;
        error["code"] = web::json::value::number(401);
        error["message"] = web::json::value::string(
            "Missing Authorization header"
        );
        request.reply(web::http::status_codes::Unauthorized, error);
        return false;
    }

    if (!m_authMiddleware->validateRequest(authHeader->second, userId))
    {
        web::json::value error;
        error["code"] = web::json::value::number(401);
        error["message"] = web::json::value::string(
            "Invalid or expired token"
        );
        request.reply(web::http::status_codes::Unauthorized, error);
        return false;
    }

    return true;
}

void RestServer::setAuthMiddleware(std::shared_ptr<AuthMiddleware> middleware)
{
    m_authMiddleware = middleware;
    LOG_INFO << "Auth middleware set";
}

bool RestServer::start()
{
    if (m_isRunning)
    {
        LOG_ERROR << "REST server is already running";
        return false;
    }

    LOG_INFO << "Starting REST server on " << m_host << ":" << m_port << "...";

    try
    {
        m_listener->open().wait();
        m_isRunning = true;
        LOG_INFO
            << "REST server started successfully on "
            << m_host << ":" << m_port;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Failed to start server: " << e.what();
        return false;
    }

    return true;
}

void RestServer::stop()
{
    if (!m_isRunning)
    {
        return;
    }

    LOG_INFO << "Stopping REST server...";
    m_isRunning = false;

    try
    {
        if (m_listener)
        {
            m_listener->close().wait();
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Error stopping server: " << e.what();
    }

    LOG_INFO << "REST server stopped";
}

} // namespace server
} // namespace farado
