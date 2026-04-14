#include <filesystem>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

#include "log.h"

namespace
{

struct LogSettings final
{
    bool needFileSourceLocation = false;
    bool needFileThreadId = false;
    std::string fileDateTimeFormat;
    std::string fileSeverityLevel;
    bool needConsoleSourceLocation = false;
    bool needConsoleThreadId = false;
    std::string consoleDateTimeFormat;
} settings;

const std::string Separator(" | ");

void consoleFormatter(
    const boost::log::record_view& record,
    boost::log::formatting_ostream& formattingStream
)
{
    using namespace boost::log;

    const auto severity = record[trivial::severity];
    if (!severity)
        return;

    const auto severityValue = severity.get();
    switch (severityValue)
    {
    case trivial::severity_level::trace:
        formattingStream
            << "\033[90m"
            << "T"
            << "\033[0m";
        break;
    case trivial::severity_level::debug:
        formattingStream
            << "\033[97m"
            << "D"
            << "\033[0m";
        break;
    case trivial::severity_level::info:
        formattingStream
            << "\033[32m"
            << "I"
            << "\033[0m";
        break;
    case trivial::severity_level::warning:
        formattingStream
            << "\033[33m"
            << "W"
            << "\033[0m";
        break;
    case trivial::severity_level::error:
        formattingStream
            << "\033[31m"
            << "E"
            << "\033[0m";
        break;
    case trivial::severity_level::fatal:
        formattingStream
            << "\033[31m"
            << "F";
        break;
    default:
        return;
    }

    formattingStream << Separator;
    auto timeStamp = expressions::format_date_time<boost::posix_time::ptime>(
        "TimeStamp", settings.consoleDateTimeFormat
    );
    const auto dateTimeFormatter = expressions::stream << timeStamp;
    dateTimeFormatter(record, formattingStream);

    if (settings.needConsoleThreadId)
    {
        formattingStream << Separator;
        auto threadId = expressions::attr<attributes::current_thread_id::value_type>(
            "ThreadID"
        );
        const auto threadFormatter = expressions::stream
            << std::hex << std::setw(16) << std::setfill('0') << threadId;
        threadFormatter(record, formattingStream);
    }

    formattingStream << Separator;
    formattingStream << record[expressions::smessage];

    if (settings.needConsoleSourceLocation)
    {
        formattingStream << Separator;
        const auto sourcePathFormatter = expressions::stream
            << expressions::attr<std::string>("File")
            << ":" << expressions::attr<int>("Line")
            << ":" << expressions::attr<std::string>("Function");
        sourcePathFormatter(record, formattingStream);
    }

    if (trivial::severity_level::fatal == severityValue)
        formattingStream << "\033[0m";
}

void fileFormatter(
    const boost::log::record_view& record,
    boost::log::formatting_ostream& formattingStream
)
{
    using namespace boost::log;

    const auto severity = record[trivial::severity];
    if (!severity)
        return;

    const auto severityValue = severity.get();
    switch (severityValue)
    {
    case trivial::severity_level::trace:
        formattingStream << "[T]";
        break;
    case trivial::severity_level::debug:
        formattingStream << "[D]";
        break;
    case trivial::severity_level::info:
        formattingStream << "[I]";
        break;
    case trivial::severity_level::warning:
        formattingStream << "[W]";
        break;
    case trivial::severity_level::error:
        formattingStream << "[E]";
        break;
    case trivial::severity_level::fatal:
        formattingStream << "[F]";
        break;
    default:
        return;
    }

    formattingStream << Separator;
    auto timeStamp = expressions::format_date_time<boost::posix_time::ptime>(
        "TimeStamp", settings.fileDateTimeFormat
    );
    const auto dateTimeFormatter = expressions::stream << timeStamp;
    dateTimeFormatter(record, formattingStream);

    if (settings.needFileThreadId)
    {
        formattingStream << Separator;
        auto threadId = expressions::attr<attributes::current_thread_id::value_type>(
            "ThreadID"
        );
        const auto threadFormatter = expressions::stream
            << std::hex << std::setw(16) << std::setfill('0') << threadId
            << std::dec;
        threadFormatter(record, formattingStream);
    }

    formattingStream << Separator;
    formattingStream << record[expressions::smessage];

    if (settings.needFileSourceLocation)
    {
        formattingStream << Separator;
        const auto sourcePathFormatter = expressions::stream
            << expressions::attr<std::string>("File")
            << ":" << expressions::attr<int>("Line")
            << ":" << expressions::attr<std::string>("Function");
        sourcePathFormatter(record, formattingStream);
    }
}

boost::log::trivial::severity_level level(const std::string& value)
{
    if (value == "trace")
        return boost::log::trivial::severity_level::trace;
    if (value == "debug")
        return boost::log::trivial::severity_level::debug;
    if (value == "info")
        return boost::log::trivial::severity_level::info;
    if (value == "warning")
        return boost::log::trivial::severity_level::warning;
    if (value == "error")
        return boost::log::trivial::severity_level::error;
    if (value == "fatal")
        return boost::log::trivial::severity_level::fatal;

    return boost::log::trivial::severity_level::info;
}

} // namespace

void initLog(
    const std::string& serviceName,
    const std::string& logPath,
    unsigned rotationSize,
    bool needFileSourceLocation,
    bool needFileThreadId,
    const std::string& fileDateTimeFormat,
    const std::string& fileSeverityLevel,
    bool needConsoleSourceLocation,
    bool needConsoleThreadId,
    const std::string& consoleDateTimeFormat,
    const std::string& consoleSeverityLevel
)
{
    settings.needFileSourceLocation = needFileSourceLocation;
    settings.needFileThreadId = needFileThreadId;
    settings.fileDateTimeFormat = fileDateTimeFormat;
    settings.fileSeverityLevel = fileSeverityLevel;
    settings.needConsoleSourceLocation = needConsoleSourceLocation;
    settings.needConsoleThreadId = needConsoleThreadId;
    settings.consoleDateTimeFormat = consoleDateTimeFormat;

    // Если пути к логу не существует, он создаётся.
    namespace fs = std::filesystem;
    const fs::path currentLogPath = fs::path(logPath) / fs::path(serviceName);
    if (!fs::exists(currentLogPath.parent_path()))
        fs::create_directories(currentLogPath.parent_path());

    using namespace boost::log;
    add_common_attributes();

    // Инициализация системы логирования для консоли.
    const auto consoleSeverity = expressions::attr<trivial::severity_level>("Severity")
        >= level(consoleSeverityLevel);
    auto consoleLogger = add_console_log(
        std::cout, keywords::filter = consoleSeverity
    );
    consoleLogger->set_formatter(&consoleFormatter);

    // Инициализация системы логирования в файл.
    const auto fileSeverity = expressions::attr<trivial::severity_level>("Severity")
        >= level(fileSeverityLevel);
    auto fileLogger = add_file_log(
        keywords::file_name = currentLogPath.string() + "_%Y%m%d_%H%M%S_%N.log",
        keywords::rotation_size = rotationSize,
        keywords::max_size = 100 * 1024 * 1024,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
        keywords::filter = fileSeverity
    );
    fileLogger->set_formatter(&fileFormatter);

    core::get()->set_filter(trivial::severity >= trivial::trace);
}
