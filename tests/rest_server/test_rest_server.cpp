// ============================================================================
// Модульные тесты REST сервера Vault
// Проверяет аутентификацию, авторизацию, CRUD операции, маршрутизацию и нагрузку
// ============================================================================

#define BOOST_TEST_MODULE "Vault REST Server Tests"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include <boost/test/unit_test.hpp>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include "vault-server/src/api/middleware/auth_middleware.h"
#include "vault-server/src/api/rest_server.h"

#include "test_server_fixture.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;

namespace farado::server::test
{

// ============================================================================
// ТЕСТЫ АУТЕНТИФИКАЦИИ И АВТОРИЗАЦИИ
// ============================================================================
// Проверяют механизмы безопасности: доступ к публичным эндпоинтам,
// защиту приватных маршрутов, валидацию токенов и выход из системы
// ============================================================================

BOOST_AUTO_TEST_SUITE(RestServerAuthTests)

/**
 * @brief Проверка доступа к публичным эндпоинтам без авторизации
 *
 * Убеждаемся, что:
 * - /health доступен без токена
 * - /auth/login возвращает токен при валидных учетных данных
 */
BOOST_FIXTURE_TEST_CASE(test_public_endpoints_no_auth_required, TestServerFixture)
{
    auto client = createClient();

    // Проверка эндпоинта health check — всегда должен быть доступен
    {
        http_request request { methods::GET };
        request.set_request_uri("/health");

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

        auto body = response.extract_json().get();
        BOOST_CHECK_EQUAL(getJsonString(body, "status"), "ok");
        BOOST_CHECK(hasJsonField(body, "timestamp"));
    }

    // Проверка эндпоинта логина — должен возвращать JWT токен
    {
        http_request request { methods::POST };
        request.set_request_uri("/auth/login");

        json::value loginBody;
        loginBody["login"] = json::value::string("admin");
        loginBody["password"] = json::value::string("admin123");
        request.set_body(loginBody);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

        auto body = response.extract_json().get();
        BOOST_CHECK(hasJsonField(body, "access_token"));
        BOOST_CHECK_EQUAL(getJsonString(body, "token_type"), "Bearer");
        BOOST_CHECK_EQUAL(getJsonInt(body, "expires_in"), 3600);
    }
}

/**
 * @brief Проверка защиты приватных эндпоинтов
 *
 * Убеждаемся, что защищенные маршруты:
 * - Отклоняют запросы без токена (401)
 * - Отклоняют запросы с неверным токеном (401)
 * - Отклоняют запросы с неверным форматом заголовка (401)
 */
BOOST_FIXTURE_TEST_CASE(test_protected_endpoints_require_auth, TestServerFixture)
{
    auto client = createClient();

    // Запрос без токена должен быть отклонен с кодом 401
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items");

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::Unauthorized);

        auto body = response.extract_json().get();
        BOOST_CHECK(hasJsonField(body, "message"));
    }

    // Запрос с неверным (испорченным) токеном
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer invalid.token.here");

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::Unauthorized);
    }

    // Запрос с неверным форматом заголовка Authorization
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "InvalidFormat");

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::Unauthorized);
    }
}

/**
 * @brief Проверка доступа к защищенным ресурсам с валидным токеном
 */
BOOST_FIXTURE_TEST_CASE(test_protected_endpoints_with_valid_token, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    http_request request { methods::GET };
    request.set_request_uri("/api/items");
    request.headers().add("Authorization", "Bearer " + token);

    auto response = client.request(request).get();
    BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

    auto body = response.extract_json().get();
    BOOST_CHECK(hasJsonField(body, "items"));
    BOOST_CHECK(hasJsonField(body, "totalCount"));
    BOOST_CHECK(hasJsonField(body, "page"));
    BOOST_CHECK(hasJsonField(body, "pageSize"));
}

/**
 * @brief Полный цикл: логин -> использование токена -> выход
 *
 * Проверяет:
 * 1. Получение токена через /auth/login
 * 2. Успешный доступ к ресурсу с полученным токеном
 * 3. Инвалидацию токена через /auth/logout
 * 4. Невозможность использовать инвалидированный токен
 */
BOOST_FIXTURE_TEST_CASE(test_token_validation_and_invalidation, TestServerFixture)
{
    auto client = createClient();

    // 1. Получаем токен через логин
    http_request loginRequest { methods::POST };
    loginRequest.set_request_uri("/auth/login");

    json::value loginBody;
    loginBody["login"] = json::value::string("admin");
    loginBody["password"] = json::value::string("admin123");
    loginRequest.set_body(loginBody);

    auto loginResponse = client.request(loginRequest).get();
    BOOST_CHECK_EQUAL(loginResponse.status_code(), status_codes::OK);

    auto loginBodyResp = loginResponse.extract_json().get();
    std::string token = getJsonString(loginBodyResp, "access_token");

    // 2. Используем токен для доступа к защищенному ресурсу
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);
    }

    // 3. Выход из системы — инвалидация токена
    {
        http_request logoutRequest { methods::POST };
        logoutRequest.set_request_uri("/auth/logout");
        logoutRequest.headers().add("Authorization", "Bearer " + token);

        auto logoutResponse = client.request(logoutRequest).get();
        BOOST_CHECK_EQUAL(logoutResponse.status_code(), status_codes::NoContent);
    }

    // 4. Инвалидированный токен больше не должен работать
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::Unauthorized);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ТЕСТЫ CRUD ОПЕРАЦИЙ С ЭЛЕМЕНТАМИ
// ============================================================================
// Проверяют базовые операции над сущностями:
// - Получение списка с пагинацией
// - Получение одного элемента
// - Создание нового элемента
// - Обновление существующего элемента
// - Удаление элемента
// ============================================================================

BOOST_AUTO_TEST_SUITE(RestServerItemsTests)

/**
 * @brief Проверка получения списка элементов с пагинацией
 *
 * Тестирует параметры запроса page и pageSize:
 * - Значения по умолчанию (page=1, pageSize=20)
 * - Кастомные значения пагинации
 * - Корректность возвращаемых ID элементов
 */
BOOST_FIXTURE_TEST_CASE(test_get_items_list_with_pagination, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    // Проверка с параметрами по умолчанию
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

        auto body = response.extract_json().get();
        BOOST_CHECK_EQUAL(getJsonInt(body, "page"), 1);
        BOOST_CHECK_EQUAL(getJsonInt(body, "pageSize"), 20);
        BOOST_CHECK_EQUAL(getJsonInt(body, "totalCount"), 100);
        BOOST_CHECK_EQUAL(body.at("items").as_array().size(), 20);
    }

    // Проверка с кастомными параметрами пагинации
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items?page=2&pageSize=10");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

        auto body = response.extract_json().get();
        BOOST_CHECK_EQUAL(getJsonInt(body, "page"), 2);
        BOOST_CHECK_EQUAL(getJsonInt(body, "pageSize"), 10);
        BOOST_CHECK_EQUAL(body.at("items").as_array().size(), 10);

        // Проверяем, что вернулись элементы с правильными ID (с 11 по 20)
        auto items = body.at("items").as_array();
        BOOST_CHECK_EQUAL(getJsonInt(items[0], "id"), 11);
        BOOST_CHECK_EQUAL(getJsonInt(items[9], "id"), 20);
    }
}

/**
 * @brief Проверка получения одного элемента по ID
 */
BOOST_FIXTURE_TEST_CASE(test_get_single_item, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    // Получение существующего элемента
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items/42");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

        auto body = response.extract_json().get();
        BOOST_CHECK_EQUAL(getJsonInt(body, "id"), 42);
        BOOST_CHECK_EQUAL(getJsonString(body, "caption"), "Item 42");
        BOOST_CHECK(hasJsonField(body, "content"));
        BOOST_CHECK(hasJsonField(body, "stateId"));
        BOOST_CHECK(hasJsonField(body, "phaseId"));
    }

    // Неверный формат ID (не число) — должно вернуться 404 или 400
    {
        http_request request { methods::GET };
        request.set_request_uri("/api/items/invalid");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        // TODO: поправить rest_server чтобы возвращал status_codes::BadRequest
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::NotFound);
    }
}

/**
 * @brief Проверка создания нового элемента
 *
 * Тестирует:
 * - Успешное создание с валидными данными
 * - Ошибку при отсутствии обязательного поля caption
 */
BOOST_FIXTURE_TEST_CASE(test_create_item, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    // Создание элемента с валидными данными
    {
        json::value itemData;
        itemData["caption"] = json::value::string("New Test Item");
        itemData["content"] = json::value::string("Test content");

        http_request request { methods::POST };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer " + token);
        request.set_body(itemData);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::Created);

        auto body = response.extract_json().get();
        BOOST_CHECK(hasJsonField(body, "id"));
        BOOST_CHECK_EQUAL(getJsonString(body, "caption"), "New Test Item");
        BOOST_CHECK_EQUAL(body.at("success").as_bool(), true);
    }

    // Создание элемента без обязательного поля caption — ожидаем ошибку
    {
        json::value itemData;
        itemData["content"] = json::value::string("Test content");

        http_request request { methods::POST };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer " + token);
        request.set_body(itemData);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::BadRequest);

        auto body = response.extract_json().get();
        std::string message = getJsonString(body, "message");
        BOOST_CHECK(message.find("caption") != std::string::npos);
    }
}

/**
 * @brief Проверка обновления существующего элемента
 */
BOOST_FIXTURE_TEST_CASE(test_update_item, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    // Обновление существующего элемента
    {
        json::value updateData;
        updateData["caption"] = json::value::string("Updated Item Name");

        http_request request { methods::PUT };
        request.set_request_uri("/api/items/42");
        request.headers().add("Authorization", "Bearer " + token);
        request.set_body(updateData);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::OK);

        auto body = response.extract_json().get();
        BOOST_CHECK_EQUAL(getJsonInt(body, "id"), 42);
        BOOST_CHECK_EQUAL(getJsonString(body, "caption"), "Updated Item Name");
        BOOST_CHECK_EQUAL(body.at("success").as_bool(), true);
    }

    // Обновление с неверным ID (не число)
    {
        json::value updateData;
        updateData["caption"] = json::value::string("Test");

        http_request request { methods::PUT };
        request.set_request_uri("/api/items/invalid");
        request.headers().add("Authorization", "Bearer " + token);
        request.set_body(updateData);

        auto response = client.request(request).get();
        // TODO: поправить rest_server чтобы возвращал status_codes::BadRequest
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::NotFound);
    }
}

/**
 * @brief Проверка удаления элемента
 */
BOOST_FIXTURE_TEST_CASE(test_delete_item, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    // Удаление существующего элемента
    {
        http_request request { methods::DEL };
        request.set_request_uri("/api/items/42");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::NoContent);
    }

    // Удаление с неверным ID (не число)
    {
        http_request request { methods::DEL };
        request.set_request_uri("/api/items/invalid");
        request.headers().add("Authorization", "Bearer " + token);

        auto response = client.request(request).get();
        // TODO: поправить rest_server чтобы возвращал status_codes::BadRequest
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::NotFound);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ТЕСТЫ МАРШРУТИЗАЦИИ И ОБРАБОТКИ ОШИБОК
// ============================================================================
// Проверяют корректность обработки несуществующих маршрутов
// и некорректного входного JSON
// ============================================================================

BOOST_AUTO_TEST_SUITE(RestServerRoutingTests)

/**
 * @brief Проверка обработки несуществующих маршрутов
 *
 * Все запросы к несуществующим эндпоинтам должны возвращать 404
 * с информативным JSON-телом ошибки
 */
BOOST_FIXTURE_TEST_CASE(test_not_found_handling, TestServerFixture)
{
    auto client = createClient();

    std::vector<std::string> invalidPaths = {
        "/nonexistent",
        "/api/wrong",
        "/invalid/route/123",
        "/api/items/extra/path"
    };

    for (const auto& path : invalidPaths)
    {
        http_request request { methods::GET };
        request.set_request_uri(path);

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::NotFound);

        auto body = response.extract_json().get();
        BOOST_CHECK_EQUAL(getJsonInt(body, "code"), 404);
        BOOST_CHECK(!getJsonString(body, "message").empty());
    }
}

/**
 * @brief Проверка обработки некорректного JSON в теле запроса
 *
 * При отправке невалидного JSON сервер должен вернуть 400 Bad Request
 */
BOOST_FIXTURE_TEST_CASE(test_malformed_json_handling, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    // Отправляем заведомо некорректный JSON
    {
        http_request request { methods::POST };
        request.set_request_uri("/api/items");
        request.headers().add("Authorization", "Bearer " + token);
        request.set_body("{invalid json}");

        auto response = client.request(request).get();
        BOOST_CHECK_EQUAL(response.status_code(), status_codes::BadRequest);
    }
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// ТЕСТЫ КОНКУРЕНТНОГО ДОСТУПА
// ============================================================================
// Проверяют поведение сервера при одновременных запросах:
// - Параллельные чтения защищенных ресурсов
// - Параллельные логин и логаут
// ============================================================================

BOOST_AUTO_TEST_SUITE(RestServerConcurrencyTests)

/**
 * @brief Проверка параллельных запросов к защищенным ресурсам
 *
 * 20 потоков одновременно запрашивают /api/items с разными страницами
 * Все запросы должны успешно завершиться
 */
BOOST_FIXTURE_TEST_CASE(test_concurrent_requests, TestServerFixture)
{
    auto client = createClient();
    std::string token = getTestToken();

    std::atomic<int> successCount { 0 };
    std::atomic<int> failureCount { 0 };
    std::vector<std::thread> threads;

    // Запускаем 20 параллельных запросов
    for (int i = 0; i < 20; ++i)
    {
        threads.emplace_back(
            [&client, &token, &successCount, &failureCount, i]()
            {
                try
                {
                    http_request request { methods::GET };
                    request.set_request_uri("/api/items?page=" + std::to_string(i % 5 + 1));
                    request.headers().add("Authorization", "Bearer " + token);

                    auto response = client.request(request).get();
                    if (response.status_code() == status_codes::OK)
                    {
                        successCount++;
                    }
                    else
                    {
                        failureCount++;
                    }
                }
                catch (const std::exception&)
                {
                    failureCount++;
                }
            }
        );
    }

    for (auto& t : threads)
    {
        t.join();
    }

    BOOST_CHECK_EQUAL(successCount, 20);
    BOOST_CHECK_EQUAL(failureCount, 0);
}

/**
 * @brief Проверка параллельных операций логина и логаута
 *
 * 10 потоков одновременно выполняют полный цикл:
 * логин -> получение токена -> логаут
 * Все операции должны успешно завершиться
 */
BOOST_FIXTURE_TEST_CASE(test_concurrent_login_logout, TestServerFixture)
{
    auto client = createClient();

    std::vector<std::thread> threads;
    std::atomic<int> loginSuccess { 0 };
    std::atomic<int> logoutSuccess { 0 };

    // Запускаем 10 потоков, каждый делает login и logout
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(
            [&client, &loginSuccess, &logoutSuccess]()
            {
                http_request loginRequest { methods::POST };
                loginRequest.set_request_uri("/auth/login");

                json::value loginBody;
                loginBody["login"] = json::value::string("admin");
                loginBody["password"] = json::value::string("admin123");
                loginRequest.set_body(loginBody);

                try
                {
                    auto loginResponse = client.request(loginRequest).get();
                    if (loginResponse.status_code() == status_codes::OK)
                    {
                        loginSuccess++;
                        auto body = loginResponse.extract_json().get();
                        std::string token = getJsonString(body, "access_token");

                        http_request logoutRequest { methods::POST };
                        logoutRequest.set_request_uri("/auth/logout");
                        logoutRequest.headers().add("Authorization", "Bearer " + token);

                        auto logoutResponse = client.request(logoutRequest).get();
                        if (logoutResponse.status_code() == status_codes::NoContent)
                        {
                            logoutSuccess++;
                        }
                    }
                }
                catch (const std::exception&)
                {
                    // В конкурентных тестах игнорируем редкие ошибки синхронизации
                }
            }
        );
    }

    for (auto& t : threads)
    {
        t.join();
    }

    BOOST_CHECK_EQUAL(loginSuccess, 10);
    BOOST_CHECK_EQUAL(logoutSuccess, 10);
}

BOOST_AUTO_TEST_SUITE_END()

// ============================================================================
// СТРЕСС-ТЕСТЫ
// ============================================================================
// Проверяют производительность и стабильность сервера
// под "высокой" нагрузкой
// ============================================================================

BOOST_AUTO_TEST_SUITE(RestServerStressTests)

/**
 * @brief Тест "высокой" нагрузки на публичный эндпоинт /health
 *
 * Выполняет 100 параллельных запросов к /health,
 * измеряет пропускную способность и проверяет успешность всех запросов
 */
BOOST_FIXTURE_TEST_CASE(test_high_load, TestServerFixture)
{
    auto client = createClient();

    const int REQUEST_COUNT = 100;
    std::atomic<int> processedCount { 0 };
    std::atomic<int> errorCount { 0 };
    std::vector<std::thread> threads;

    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < REQUEST_COUNT; ++i)
    {
        threads.emplace_back(
            [&client, &processedCount, &errorCount]()
            {
                try
                {
                    http_request request { methods::GET };
                    request.set_request_uri("/health");

                    auto response = client.request(request).get();
                    if (response.status_code() == status_codes::OK)
                    {
                        processedCount++;
                    }
                    else
                    {
                        errorCount++;
                    }
                }
                catch (const std::exception&)
                {
                    errorCount++;
                }
            }
        );
    }

    for (auto& t : threads)
    {
        t.join();
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Выводим метрики производительности в консоль для анализа
    std::cout << "Processed " << REQUEST_COUNT << " requests in " << duration.count() << "ms" << std::endl;
    std::cout << "Throughput: " << (REQUEST_COUNT * 1000.0 / duration.count()) << " req/sec" << std::endl;

    BOOST_CHECK_EQUAL(processedCount, REQUEST_COUNT);
    BOOST_CHECK_EQUAL(errorCount, 0);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace farado::server::test
