#pragma once

#include <string>
#include <vector>

#include "idatabase.h"

namespace db
{

/**
 * @brief Базовый класс для миграции базы данных.
 *
 * Каждая миграция представляет собой отдельный класс, наследующий этот интерфейс.
 * Миграции выполняются последовательно от меньшей версии к большей.
 */
class IMigration
{
public:
    virtual ~IMigration() = default;

    /**
     * @brief Выполняет миграцию (обновление до текущей версии).
     * @param connection Соединение с БД
     */
    virtual void up(std::shared_ptr<IConnection> connection) = 0;

    /**
     * @brief Откатывает миграцию (возврат к предыдущей версии).
     * @param connection Соединение с БД
     */
    virtual void down(std::shared_ptr<IConnection> connection) = 0;

    /**
     * @brief Возвращает номер версии миграции.
     * @return Номер версии (целое положительное число)
     */
    virtual unsigned int version() const = 0;

    /**
     * @brief Возвращает описание миграции.
     * @return Описание миграции
     */
    virtual std::string description() const = 0;
};

} // namespace db
