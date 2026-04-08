#pragma once

#include <memory>
#include <string>
#include <map>

#include <cpprest/http_msg.h>
#include <cpprest/json.h>

namespace farado
{
namespace server
{
namespace handlers
{

class ItemsHandler final
{
public:
    ItemsHandler();
    ~ItemsHandler() = default;

    /**
     * Получает список элементов
     */
    void handleGetItems(
        const web::http::http_request& request,
        const std::string& userId
    );

    /**
     * Получает элемент по ID
     */
    void handleGetItem(
        const web::http::http_request& request,
        const std::string& userId
    );

    /**
     * Создает новый элемент
     */
    void handleCreateItem(
        const web::http::http_request& request,
        const std::string& userId
    );

    /**
     * Обновляет существующий элемент
     */
    void handleUpdateItem(
        const web::http::http_request& request,
        const std::string& userId
    );

    /**
     * Удаляет элемент
     */
    void handleDeleteItem(
        const web::http::http_request& request,
        const std::string& userId
    );

private:
    /**
     * Извлекает ID из пути запроса
     */
    int64_t extractItemId(const web::http::http_request& request);
    
    void sendErrorResponse(
        web::http::http_response& response,
        int code,
        const std::string& message
    );

private:
    std::map<std::string, std::string> extractQueryParams(
        const web::http::http_request& request
    );
};

} // namespace handlers
} // namespace server
} // namespace farado
