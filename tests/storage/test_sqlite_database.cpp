#include <filesystem>

#include <boost/test/unit_test.hpp>

#include "vault-server/src/storage/database_factory.h"
#include "vault-server/src/storage/sqlite/sqlite_database.h"

#include "test_fixture.h"

namespace db::test
{

BOOST_AUTO_TEST_SUITE(SqliteDatabaseTests)

/**
 * @brief Проверка базовой инициализации и завершения работы с БД
 *
 * Убеждаемся, что экземпляр базы данных можно корректно инициализировать
 * с конфигурацией и затем завершить его работу без ошибок.
 */
BOOST_AUTO_TEST_CASE(test_initialize_and_shutdown)
{
    SqliteDatabase db;
    DatabaseConfig config;
    config["database"] = getTestDbPath("test_init.db");

    BOOST_REQUIRE_NO_THROW(db.initialize(config));
    BOOST_REQUIRE_NO_THROW(db.shutdown());
}

/**
 * @brief Проверка, что повторная инициализация вызывает исключение
 *
 * База данных не должна позволять инициализировать себя дважды без
 * промежуточного shutdown().
 */
BOOST_AUTO_TEST_CASE(test_initialize_twice_throws)
{
    SqliteDatabase db;
    DatabaseConfig config;
    config["database"] = getTestDbPath("test_init_twice.db");

    db.initialize(config);
    BOOST_CHECK_THROW(db.initialize(config), std::runtime_error);
    db.shutdown();
}

/**
 * @brief Проверка, что инициализация без пути к файлу вызывает исключение
 *
 * Конфигурация должна содержать ключ "database" с путём к файлу БД.
 */
BOOST_AUTO_TEST_CASE(test_initialize_without_config_throws)
{
    SqliteDatabase db;
    DatabaseConfig config; // Пустая конфигурация, нет ключа "database"
    BOOST_CHECK_THROW(db.initialize(config), std::runtime_error);
}

/**
 * @brief Проверка, что метод connection() возвращает рабочее соединение
 */
BOOST_AUTO_TEST_CASE(test_connection_returns_valid_connection)
{
    SqliteDatabase db;
    DatabaseConfig config;
    config["database"] = getTestDbPath("test_connection.db");
    db.initialize(config);

    auto conn = db.connection();
    BOOST_REQUIRE(conn != nullptr);

    // Проверяем, что соединение действительно работает
    BOOST_CHECK_NO_THROW(conn->execute("CREATE TABLE test (id INTEGER)"));

    db.shutdown();
}

/**
 * @brief Проверка, что соединение переиспользуется через weak_ptr
 *
 * Наша реализация использует weak_ptr для отслеживания активного соединения.
 * Если соединение ещё живо — возвращается оно же, а не создается новое.
 * После освобождения всех ссылок соединение автоматически закрывается,
 * и следующий запрос создаст новое соединение.
 */
BOOST_AUTO_TEST_CASE(test_connection_reuse_via_weak_ptr)
{
    SqliteDatabase db;
    DatabaseConfig config;
    config["database"] = getTestDbPath("test_conn_reuse.db");
    db.initialize(config);

    // Первый запрос — создает новое соединение
    auto conn1 = db.connection();
    BOOST_REQUIRE(conn1 != nullptr);

    // Второй запрос — возвращает ТО ЖЕ соединение (пока conn1 жив)
    auto conn2 = db.connection();
    BOOST_REQUIRE(conn2 != nullptr);

    // Проверяем, что это физически одно и то же соединение
    conn1->execute("CREATE TABLE IF NOT EXISTS reuse_test (id INTEGER)");
    conn1->execute("DELETE FROM reuse_test"); // Очищаем таблицу
    conn1->execute("INSERT INTO reuse_test VALUES (42)");

    // Через conn2 видим те же данные (значит соединение одно)
    auto stmt = conn2->prepareStatement("SELECT id FROM reuse_test");
    auto rs = stmt->executeQuery();
    BOOST_REQUIRE(rs->next());
    BOOST_CHECK_EQUAL(rs->valueInt64(0), 42);

    // Проверяем, что conn1 и conn2 указывают на одно соединение
    // (можно проверить через транзакцию)
    auto tx = conn1->beginTransaction();
    conn1->execute("UPDATE reuse_test SET id = 100");

    // conn2 видит незакоммиченные изменения (значит соединение одно)
    auto stmt2 = conn2->prepareStatement("SELECT id FROM reuse_test");
    auto rs2 = stmt2->executeQuery();
    BOOST_REQUIRE(rs2->next());
    BOOST_CHECK_EQUAL(rs2->valueInt64(0), 100);

    tx->rollback();

    // Освобождаем все ссылки на соединение
    conn1.reset();
    conn2.reset();

    // Теперь weak_ptr expired, следующий запрос создаст НОВОЕ соединение
    auto conn3 = db.connection();
    BOOST_REQUIRE(conn3 != nullptr);

    // Новое соединение видит таблицу (она в файле БД)
    auto stmt3 = conn3->prepareStatement("SELECT id FROM reuse_test");
    auto rs3 = stmt3->executeQuery();
    BOOST_REQUIRE(rs3->next());
    BOOST_CHECK_EQUAL(rs3->valueInt64(0), 42); // Данные после rollback

    db.shutdown();
}

/**
 * @brief Проверка автоматического закрытия соединения при освобождении всех ссылок
 *
 * Когда все shared_ptr на соединение освобождены, weak_ptr::lock()
 * возвращает nullptr, и база данных может создать новое соединение.
 */
BOOST_AUTO_TEST_CASE(test_connection_auto_close_on_release)
{
    SqliteDatabase db;
    DatabaseConfig config;
    config["database"] = getTestDbPath("test_auto_close.db");
    db.initialize(config);

    // Блок с ограниченной областью видимости
    {
        auto conn = db.connection();
        conn->execute("CREATE TABLE test_table (data TEXT)");
        conn->execute("INSERT INTO test_table VALUES ('persistent')");
        // conn автоматически освобождается при выходе из блока
    }

    // Соединение закрылось, но данные в файле сохранились
    // Создаем новое соединение и проверяем
    auto newConn = db.connection();
    auto stmt = newConn->prepareStatement("SELECT data FROM test_table");
    auto rs = stmt->executeQuery();
    rs->next();
    BOOST_CHECK_EQUAL(rs->valueString(0), "persistent");

    db.shutdown();
}

/**
 * @brief Проверка, что запрос соединения до инициализации вызывает исключение
 */
BOOST_AUTO_TEST_CASE(test_connection_before_initialize_throws)
{
    SqliteDatabase db;
    // Не вызывали initialize()
    BOOST_CHECK_THROW(db.connection(), std::runtime_error);
}

/**
 * @brief Проверка фабрики: создание SQLite-базы данных через DatabaseFactory
 *
 * Убеждаемся, что фабрика корректно создаёт экземпляр БД нужного типа
 * и что с ним можно работать через универсальный интерфейс.
 */
BOOST_AUTO_TEST_CASE(test_factory_creates_sqlite)
{
    auto db = DatabaseFactory::create(DatabaseType::Sqlite);
    BOOST_REQUIRE(db != nullptr);

    DatabaseConfig config;
    config["database"] = getTestDbPath("test_factory.db");
    db->initialize(config);

    auto conn = db->connection();
    conn->execute("CREATE TABLE factory_test (id INTEGER)");
    conn->execute("INSERT INTO factory_test VALUES (42)");

    auto stmt = conn->prepareStatement("SELECT id FROM factory_test");
    auto rs = stmt->executeQuery();
    rs->next();
    BOOST_CHECK_EQUAL(rs->valueInt64(0), 42);

    db->shutdown();
}

/**
 * @brief Проверка фабрики: создание БД из строки подключения
 *
 * Строка подключения имеет формат: "sqlite://путь/к/файлу.db"
 * Метод должен распарсить её и создать соответствующий экземпляр.
 */
BOOST_AUTO_TEST_CASE(test_factory_from_connection_string)
{
    const std::string dbPath = "./test_data/test_conn_string.db";
    const std::string connStr = "sqlite://" + dbPath.substr(2); // "sqlite://test_data/test_conn_string.db"

    std::error_code ec;
    std::filesystem::remove(dbPath, ec);

    auto db = DatabaseFactory::createFromConnectionString(connStr);
    BOOST_REQUIRE(db != nullptr);

    auto conn = db->connection();
    conn->execute("CREATE TABLE conn_test (data TEXT)");
    conn->execute("INSERT INTO conn_test VALUES ('test')");

    auto stmt = conn->prepareStatement("SELECT data FROM conn_test");
    auto rs = stmt->executeQuery();
    rs->next();
    BOOST_CHECK_EQUAL(rs->valueString(0), "test");

    db->shutdown();
    std::filesystem::remove(dbPath, ec);
}

/**
 * @brief Проверка, что некорректная строка подключения вызывает исключение
 */
BOOST_AUTO_TEST_CASE(test_factory_from_invalid_connection_string_throws)
{
    std::string invalidConnStr = "invalid://path/to/db";
    BOOST_CHECK_THROW(
        DatabaseFactory::createFromConnectionString(invalidConnStr),
        std::runtime_error
    );
}

/**
 * @brief Проверка, что запрос неподдерживаемого типа БД вызывает исключение
 */
BOOST_AUTO_TEST_CASE(test_factory_unsupported_type_throws)
{
    BOOST_CHECK_THROW(
        DatabaseFactory::create(static_cast<DatabaseType>(999)),
        std::runtime_error
    );
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace db::test
