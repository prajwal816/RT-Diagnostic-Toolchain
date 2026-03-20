#pragma once

/// @file logger.h
/// @brief Lightweight, thread-safe diagnostic logger.

#include <cstdint>
#include <mutex>
#include <string>

namespace rtdiag {
namespace utils {

/// Log severity levels.
enum class LogLevel : uint8_t {
    kDebug = 0,
    kInfo,
    kWarn,
    kError,
};

/// Thread-safe diagnostic logger with file and stderr output.
class Logger {
public:
    /// Get the global logger instance.
    static Logger& Instance();

    /// Set minimum log level (messages below this level are discarded).
    void SetLevel(LogLevel level);

    /// Set output file path. If empty, logs go to stderr only.
    void SetOutput(const std::string& filepath);

    /// Log a message at the given level.
    void Log(LogLevel level, const char* file, int line, const char* fmt, ...);

    /// Close the log file (if any).
    void Close();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel    level_;
    std::string filepath_;
    FILE*       file_;
    std::mutex  mutex_;
};

/// Convenience macros for logging.
#define RTDIAG_LOG_DEBUG(fmt, ...) \
    ::rtdiag::utils::Logger::Instance().Log( \
        ::rtdiag::utils::LogLevel::kDebug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define RTDIAG_LOG_INFO(fmt, ...) \
    ::rtdiag::utils::Logger::Instance().Log( \
        ::rtdiag::utils::LogLevel::kInfo, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define RTDIAG_LOG_WARN(fmt, ...) \
    ::rtdiag::utils::Logger::Instance().Log( \
        ::rtdiag::utils::LogLevel::kWarn, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define RTDIAG_LOG_ERROR(fmt, ...) \
    ::rtdiag::utils::Logger::Instance().Log( \
        ::rtdiag::utils::LogLevel::kError, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

}  // namespace utils
}  // namespace rtdiag
