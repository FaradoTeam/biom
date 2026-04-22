#pragma once

#include <memory>
#include <optional>
#include <string>

#include "../user_repository.h"

namespace db
{
class IDatabase;
class IConnection;
}

namespace server
{
namespace repositories
{

/**
 * @brief Реализация репозитория пользователей для SQLite.
 *
 * Предоставляет методы для работы с таблицей User в базе данных SQLite.
 */
class SqliteUserRepository final : public IUserRepository
{
public:
    /**
     * @brief Конструктор.
     * @param database База данных (используется для получения соединения)
     */
    explicit SqliteUserRepository(std::shared_ptr<db::IDatabase> database);
    ~SqliteUserRepository() override = default;

    SqliteUserRepository(const SqliteUserRepository&) = delete;
    SqliteUserRepository& operator=(const SqliteUserRepository&) = delete;
    SqliteUserRepository(SqliteUserRepository&&) = delete;
    SqliteUserRepository& operator=(SqliteUserRepository&&) = delete;

    std::optional<dto::User> findByLogin(const std::string& login) override;
    std::optional<dto::User> findById(int64_t id) override;
    bool updatePassword(int64_t userId, const std::string& passwordHash) override;
    bool updateNeedChangePassword(int64_t userId, bool needChange) override;
    int64_t create(const dto::User& user, const std::string& passwordHash) override;
    bool existsByLogin(const std::string& login) override;

    /**
     * @brief Получает соединение с БД.
     */
    std::shared_ptr<db::IConnection> getConnection() const;

private:
    std::shared_ptr<db::IDatabase> m_database;
};

} // namespace repositories
} // namespace server
