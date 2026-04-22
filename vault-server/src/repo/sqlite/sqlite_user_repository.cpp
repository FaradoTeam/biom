#include <stdexcept>

#include "common/log/log.h"

#include "storage/idatabase.h"

#include "sqlite_user_repository.h"

namespace
{
/**
 * @brief Преобразует строку результата в объект User.
 */
dto::User mapRowToUser(db::IResultSet& rs)
{
    dto::User user;
    user.id = rs.valueInt64("id");
    user.login = rs.valueString("login");

    // Опциональные поля могут быть NULL
    if (!rs.isNull("firstName"))
        user.firstName = rs.valueString("firstName");
    if (!rs.isNull("middleName"))
        user.middleName = rs.valueString("middleName");
    if (!rs.isNull("lastName"))
        user.lastName = rs.valueString("lastName");

    user.email = rs.valueString("email");

    // Добавлено чтение needChangePassword
    user.needChangePassword = rs.valueInt64("needChangePassword") != 0;

    user.isBlocked = rs.valueInt64("isBlocked") != 0;
    user.isSuperAdmin = rs.valueInt64("isSuperAdmin") != 0;
    user.isHidden = rs.valueInt64("isHidden") != 0;

    return user;
}
}

namespace server
{
namespace repositories
{

SqliteUserRepository::SqliteUserRepository(std::shared_ptr<db::IDatabase> database)
    : m_database(std::move(database))
{
    if (!m_database)
    {
        throw std::runtime_error("SqliteUserRepository: database is null");
    }
}

std::optional<dto::User> SqliteUserRepository::findByLogin(const std::string& login)
{
    if (login.empty())
    {
        LOG_WARN << "findByLogin: empty login provided";
        return std::nullopt;
    }

    try
    {
        auto conn = m_database->connection();
        auto stmt = conn->prepareStatement(
            "SELECT id, login, firstName, middleName, lastName, email, "
            "needChangePassword, isBlocked, isSuperAdmin, isHidden "
            "FROM User WHERE login = :login"
        );

        stmt->bindString("login", login);
        auto rs = stmt->executeQuery();

        if (rs->next())
        {
            return mapRowToUser(*rs);
        }

        return std::nullopt;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "findByLogin failed: " << e.what();
        throw;
    }
}

std::optional<dto::User> SqliteUserRepository::findById(int64_t id)
{
    if (id <= 0)
    {
        LOG_WARN << "findById: invalid id " << id;
        return std::nullopt;
    }

    try
    {
        auto conn = m_database->connection();
        auto stmt = conn->prepareStatement(
            "SELECT id, login, firstName, middleName, lastName, email, "
            "needChangePassword, isBlocked, isSuperAdmin, isHidden "
            "FROM User WHERE id = :id"
        );

        stmt->bindInt64("id", id);
        auto rs = stmt->executeQuery();

        if (rs->next())
        {
            return mapRowToUser(*rs);
        }

        return std::nullopt;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "findById failed: " << e.what();
        throw;
    }
}

bool SqliteUserRepository::updatePassword(int64_t userId, const std::string& passwordHash)
{
    if (userId <= 0 || passwordHash.empty())
    {
        LOG_WARN << "updatePassword: invalid parameters";
        return false;
    }

    try
    {
        auto conn = m_database->connection();
        auto stmt = conn->prepareStatement(
            "UPDATE User SET passwordHash = :passwordHash WHERE id = :id"
        );

        stmt->bindString("passwordHash", passwordHash);
        stmt->bindInt64("id", userId);

        int64_t affected = stmt->execute();
        return affected > 0;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "updatePassword failed: " << e.what();
        return false;
    }
}

bool SqliteUserRepository::updateNeedChangePassword(int64_t userId, bool needChange)
{
    if (userId <= 0)
    {
        LOG_WARN << "updateNeedChangePassword: invalid userId " << userId;
        return false;
    }

    try
    {
        auto conn = m_database->connection();
        auto stmt = conn->prepareStatement(
            "UPDATE User SET needChangePassword = :needChange WHERE id = :id"
        );

        stmt->bindInt64("needChange", needChange ? 1 : 0);
        stmt->bindInt64("id", userId);

        int64_t affected = stmt->execute();
        return affected > 0;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "updateNeedChangePassword failed: " << e.what();
        return false;
    }
}

int64_t SqliteUserRepository::create(const dto::User& user, const std::string& passwordHash)
{
    // Проверка обязательных полей с учётом optional
    if (!user.login.has_value() || user.login->empty() || !user.email.has_value() || user.email->empty() || passwordHash.empty())
    {
        LOG_WARN << "create: required fields missing";
        return 0;
    }

    try
    {
        auto conn = m_database->connection();
        auto stmt = conn->prepareStatement(
            "INSERT INTO User (login, firstName, middleName, lastName, email, "
            "passwordHash, needChangePassword, isBlocked, isSuperAdmin, isHidden) "
            "VALUES (:login, :firstName, :middleName, :lastName, :email, "
            ":passwordHash, :needChangePassword, :isBlocked, :isSuperAdmin, :isHidden)"
        );

        stmt->bindString("login", user.login.value());

        if (user.firstName.has_value())
            stmt->bindString("firstName", *user.firstName);
        else
            stmt->bindNull("firstName");

        if (user.middleName.has_value())
            stmt->bindString("middleName", *user.middleName);
        else
            stmt->bindNull("middleName");

        if (user.lastName.has_value())
            stmt->bindString("lastName", *user.lastName);
        else
            stmt->bindNull("lastName");

        stmt->bindString("email", user.email.value());
        stmt->bindString("passwordHash", passwordHash);
        stmt->bindInt64("needChangePassword", user.needChangePassword.value_or(true) ? 1 : 0);
        stmt->bindInt64("isBlocked", user.isBlocked.value_or(false) ? 1 : 0);
        stmt->bindInt64("isSuperAdmin", user.isSuperAdmin.value_or(false) ? 1 : 0);
        stmt->bindInt64("isHidden", user.isHidden.value_or(false) ? 1 : 0);

        stmt->execute();
        return conn->lastInsertId();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "create failed: " << e.what();
        throw; // Пробрасываем исключение для обработки в вызывающем коде
    }
}

bool SqliteUserRepository::existsByLogin(const std::string& login)
{
    if (login.empty())
        return false;

    try
    {
        auto conn = m_database->connection();
        auto stmt = conn->prepareStatement(
            "SELECT 1 FROM User WHERE login = :login LIMIT 1"
        );

        stmt->bindString("login", login);
        auto rs = stmt->executeQuery();

        return rs->next();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "existsByLogin failed: " << e.what();
        return false;
    }
}

std::shared_ptr<db::IConnection> SqliteUserRepository::getConnection() const
{
    return m_database->connection();
}

} // namespace repositories
} // namespace server
