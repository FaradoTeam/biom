#pragma once

#include <memory>
#include <string>
#include <vector>

#include "migration.h"

namespace db
{

/**
 * @brief Управляет миграциями базы данных.
 *
 * Отвечает за хранение списка миграций, определение текущей версии БД,
 * выполнение миграций вперёд и назад.
 */
class MigrationManager final
{
public:
    /**
     * @brief Конструктор.
     * @param connection Соединение с БД
     */
    explicit MigrationManager(std::shared_ptr<IConnection> connection);

    // Запрещаем копирование
    MigrationManager(const MigrationManager&) = delete;
    MigrationManager& operator=(const MigrationManager&) = delete;

    /**
     * @brief Регистрирует миграцию.
     * @param migration Уникальный указатель на миграцию
     */
    void registerMigration(std::unique_ptr<IMigration> migration);

    /**
     * @brief Инициализирует таблицу .schema, если она не существует.
     * @throws std::runtime_error При ошибке создания таблицы
     */
    void initializeSchemaTable();

    /**
     * @brief Возвращает текущую версию базы данных.
     * @return Текущая версия (0, если таблица пуста)
     * @throws std::runtime_error При ошибке чтения
     */
    unsigned int currentVersion() const;

    /**
     * @brief Устанавливает текущую версию базы данных.
     * @param version Новая версия
     * @throws std::runtime_error При ошибке записи
     */
    void setCurrentVersion(unsigned int version);

    /**
     * @brief Возвращает список всех доступных миграций.
     * @return Вектор пар (версия, описание)
     */
    std::vector<std::pair<unsigned int, std::string>> availableMigrations() const;

    /**
     * @brief Выполняет одну миграцию вверх.
     * @throws std::runtime_error Если нет доступных миграций для обновления
     */
    void upgradeOne();

    /**
     * @brief Выполняет все миграции до последней версии.
     * @throws std::runtime_error При ошибке выполнения
     */
    void upgradeAll();

    /**
     * @brief Откатывает одну миграцию вниз.
     * @throws std::runtime_error Если текущая версия 0 или нет миграции для отката
     */
    void downgradeOne();

    /**
     * @brief Возвращает максимальную доступную версию миграции.
     * @return Максимальная версия
     */
    unsigned int maxVersion() const;

private:
    /**
     * @brief Находит миграцию по номеру версии.
     * @param version Номер версии
     * @return Указатель на миграцию или nullptr
     */
    IMigration* findMigration(unsigned int version);

private:
    /// Соединение с БД
    std::shared_ptr<IConnection> m_connection;

    /// Список миграций
    std::vector<std::unique_ptr<IMigration>> m_migrations;
};

} // namespace db
