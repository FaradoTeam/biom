#include <algorithm>
#include <stdexcept>

#include "migration_manager.h"

namespace db
{

MigrationManager::MigrationManager(std::shared_ptr<IConnection> connection)
    : m_connection(std::move(connection))
{
    if (!m_connection)
    {
        throw std::runtime_error("MigrationManager: connection is null");
    }
}

void MigrationManager::registerMigration(std::unique_ptr<IMigration> migration)
{
    if (!migration)
    {
        throw std::runtime_error("Cannot register null migration");
    }

    // Проверяем уникальность версии
    const unsigned int version = migration->version();
    for (const auto& m : m_migrations)
    {
        if (m->version() == version)
        {
            throw std::runtime_error(
                "Migration version " + std::to_string(version) + " already registered"
            );
        }
    }

    m_migrations.push_back(std::move(migration));

    // Сортируем миграции по версии
    std::sort(
        m_migrations.begin(),
        m_migrations.end(),
        [](const std::unique_ptr<IMigration>& a, const std::unique_ptr<IMigration>& b)
        {
            return a->version() < b->version();
        }
    );
}

void MigrationManager::initializeSchemaTable()
{
    try
    {
        // Пытаемся проверить существование таблицы через запрос к системному каталогу
        // Используем стандартный INFORMATION.schema (работает в PostgreSQL и многих других)
        const std::string checkQuery = "SELECT table_name FROM information.schema.tables "
                                       "WHERE table_name = '.schema'";

        auto stmt = m_connection->prepareStatement(checkQuery);
        auto result = stmt->executeQuery();

        if (result->next())
        {
            // Таблица уже существует
            return;
        }
    }
    catch (const std::exception&)
    {
        // Если INFORMATION.schema недоступен (SQLite, MySQL старых версий),
        // пробуем альтернативный способ - просто пытаемся выполнить SELECT
        try
        {
            const std::string testQuery = "SELECT version FROM \".schema\" LIMIT 1";
            auto stmt = m_connection->prepareStatement(testQuery);
            stmt->executeQuery();
            // Если дошли сюда - таблица существует
            return;
        }
        catch (const std::exception&)
        {
            // Таблица не существует - будем создавать
        }
    }

    // Создаём таблицу .schema стандартным SQL
    const std::string createTableSQL = "CREATE TABLE \".schema\" ("
                                       "    version INTEGER NOT NULL DEFAULT 0"
                                       ")";

    m_connection->execute(createTableSQL);

    // Вставляем начальную запись
    const std::string insertSQL = "INSERT INTO \".schema\" (version) VALUES (0)";

    m_connection->execute(insertSQL);
}

unsigned int MigrationManager::currentVersion() const
{
    try
    {
        const std::string querySQL = "SELECT version FROM \".schema\" LIMIT 1";
        auto stmt = m_connection->prepareStatement(querySQL);
        auto result = stmt->executeQuery();

        if (result->next())
        {
            return static_cast<unsigned int>(result->valueInt64(0));
        }

        // Таблица пуста - возвращаем 0
        return 0;
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            "Failed to get current version: " + std::string(e.what())
        );
    }
}

void MigrationManager::setCurrentVersion(unsigned int version)
{
    try
    {
        const std::string updateSQL = "UPDATE \".schema\" SET version = "
            + std::to_string(version);
        m_connection->execute(updateSQL);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            "Failed to set version to "
            + std::to_string(version) + ": " + std::string(e.what())
        );
    }
}

std::vector<std::pair<unsigned int, std::string>>
MigrationManager::availableMigrations() const
{
    std::vector<std::pair<unsigned int, std::string>> result;
    result.reserve(m_migrations.size());
    for (const auto& migration : m_migrations)
    {
        result.emplace_back(
            migration->version(),
            migration->description()
        );
    }

    return result;
}

IMigration* MigrationManager::findMigration(unsigned int version)
{
    for (const auto& migration : m_migrations)
    {
        if (migration->version() == version)
        {
            return migration.get();
        }
    }
    return nullptr;
}

void MigrationManager::upgradeOne()
{
    const unsigned int currentVersion = this->currentVersion();
    const unsigned int maxVersion = this->maxVersion();

    if (currentVersion >= maxVersion)
    {
        throw std::runtime_error(
            "Already at latest version (" + std::to_string(currentVersion) + ")"
        );
    }

    const unsigned int targetVersion = currentVersion + 1;
    IMigration* migration = findMigration(targetVersion);

    if (!migration)
    {
        throw std::runtime_error(
            "Migration version " + std::to_string(targetVersion) + " not found"
        );
    }

    // Выполняем миграцию в транзакции
    auto transaction = m_connection->beginTransaction();

    try
    {
        migration->up(m_connection);
        setCurrentVersion(targetVersion);
        transaction->commit();
    }
    catch (const std::exception& e)
    {
        transaction->rollback();
        throw std::runtime_error(
            "Failed to upgrade to version "
            + std::to_string(targetVersion) + ": " + e.what()
        );
    }
}

void MigrationManager::upgradeAll()
{
    const unsigned int currentVersion = this->currentVersion();
    const unsigned int maxVersion = this->maxVersion();

    if (currentVersion >= maxVersion)
    {
        throw std::runtime_error(
            "Already at latest version (" + std::to_string(currentVersion) + ")"
        );
    }

    for (unsigned int targetVersion = currentVersion + 1; targetVersion <= maxVersion; ++targetVersion)
    {
        IMigration* migration = findMigration(targetVersion);
        if (!migration)
        {
            throw std::runtime_error(
                "Migration version " + std::to_string(targetVersion) + " not found"
            );
        }

        // Каждая миграция выполняется в своей транзакции
        auto transaction = m_connection->beginTransaction();

        try
        {
            migration->up(m_connection);
            setCurrentVersion(targetVersion);
            transaction->commit();
        }
        catch (const std::exception& e)
        {
            transaction->rollback();
            throw std::runtime_error(
                "Failed to upgrade to version " + std::to_string(targetVersion) + ": " + e.what()
            );
        }
    }
}

void MigrationManager::downgradeOne()
{
    const unsigned int currentVersion = this->currentVersion();

    if (currentVersion == 0)
    {
        throw std::runtime_error("Cannot downgrade: already at version 0");
    }

    const unsigned int targetVersion = currentVersion - 1;
    IMigration* migration = findMigration(currentVersion);

    if (!migration)
    {
        throw std::runtime_error(
            "Migration version " + std::to_string(currentVersion) + " not found"
        );
    }

    // Выполняем откат в транзакции
    auto transaction = m_connection->beginTransaction();

    try
    {
        migration->down(m_connection);
        setCurrentVersion(targetVersion);
        transaction->commit();
    }
    catch (const std::exception& e)
    {
        transaction->rollback();
        throw std::runtime_error(
            "Failed to downgrade from version "
            + std::to_string(currentVersion) + ": " + e.what()
        );
    }
}

unsigned int MigrationManager::maxVersion() const
{
    if (m_migrations.empty())
    {
        return 0;
    }
    return m_migrations.back()->version();
}

} // namespace db
