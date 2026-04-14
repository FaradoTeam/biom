#ifndef COMMON_LOG_H
#define COMMON_LOG_H

/*******************************************************************************
 * Средство логирования в файл и консоль.
 *
 * @note Пример использования:
 *  ```cpp
 *  #include "log.h"
 *
 *  int main()
 *  {
 *      initLog("test");
 *      LOG_TRACE << "Trace message";
 *      LOG_DEBUG << "Debug message";
 *      LOG_INFO << "Info message";
 *      LOG_WARN << "Warning message";
 *      LOG_ERROR << "Error message";
 *      LOG_FATAL << "Fatal message";
 *      return 0;
 *  }
 *  ```
*******************************************************************************/

#include <string>

#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#define LOG_LEVEL(LEVEL)                           \
    BOOST_LOG_TRIVIAL(LEVEL)                       \
        << boost::log::add_value("Line", __LINE__) \
        << boost::log::add_value("File", __FILE__) \
        << boost::log::add_value("Function", __FUNCTION__)

#define LOG_TRACE LOG_LEVEL(trace)
#define LOG_DEBUG LOG_LEVEL(debug)
#define LOG_INFO LOG_LEVEL(info)
#define LOG_WARN LOG_LEVEL(warning)
#define LOG_ERROR LOG_LEVEL(error)
#define LOG_FATAL LOG_LEVEL(fatal)

/** Добавляет логирование в файл и консоль.
 *
 * @param[in] serviceName имя сервиса, участвует в формировании имени
 *  лог-файла.
 *
 * @param[in] logPath путь к папке с логами.
 *
 * @param[in] rotationSize параметр отвечает за создание нового файла лога,
 *  когда текущий достигает заданного размера.
 *
 * @param[in] needFileSourceLocation флаг отвечает за вывод имени файла, номера
 *  строки и имени функции в исходном коде при записи в лог-файл.
 *
 * @param[in] needFileThreadId флаг отвечает за вывод идентификатора потока
 *  в лог-файл.
 *
 * @param[in] fileDateTimeFormat параметр отвечает за формат вывода даты/времени
 *  в лог-файл.
 *
 * @param[in] fileSeverityLevel минимальный уровень важности фиксируемых
 *  сообщений в лог-файл.
 *
 * @param[in] needConsoleSourceLocation флаг отвечает за вывод имени файла,
 *  номера строки и имени функции в исходном коде при записи в консоль.
 *
 * @param[in] needConsoleThreadId флаг отвечает за вывод идентификатора потока
 *  в консоль.
 *
 * @param[in] consoleDateTimeFormat параметр отвечает за формат вывода
 *  даты/времени в консоль.
 *
 * @param[in] consoleSeverityLevel минимальный уровень важности фиксируемых
 *  сообщений в консоль.
 *
 * @note Возможные варианты значений SeverityLevel:
 *  trace, debug, info, warning, error и fatal.
 *
 */
void initLog(
    const std::string& serviceName,

    // -- File -- //
    const std::string& logPath = "./logs/",
    unsigned rotationSize = 10 * 1024 * 1024, // 10 МБ.
    bool needFileSourceLocation = true,
    bool needFileThreadId = true,
    const std::string& fileDateTimeFormat = "%Y-%m-%d %H:%M:%S.%f",
    const std::string& fileSeverityLevel = "info",

    // -- Console -- //
    bool needConsoleSourceLocation = false,
    bool needConsoleThreadId = true,
    const std::string& consoleDateTimeFormat = "%Y-%m-%d %H:%M:%S.%f",
    const std::string& consoleSeverityLevel = "info"
);

#endif // COMMON_LOG_H
