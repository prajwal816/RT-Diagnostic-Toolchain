#include "logger.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace rtdiag {
namespace utils {

static const char* LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::kDebug: return "DEBUG";
        case LogLevel::kInfo:  return "INFO ";
        case LogLevel::kWarn:  return "WARN ";
        case LogLevel::kError: return "ERROR";
        default:               return "?????";
    }
}

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : level_(LogLevel::kInfo), file_(nullptr) {}

Logger::~Logger() {
    Close();
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::SetOutput(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    Close();
    if (!filepath.empty()) {
        file_ = fopen(filepath.c_str(), "a");
        filepath_ = filepath;
    }
}

void Logger::Log(LogLevel level, const char* file, int line,
                 const char* fmt, ...) {
    if (level < level_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Timestamp
    time_t now = time(nullptr);
    struct tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    // Extract basename from file path
    const char* basename = strrchr(file, '/');
    if (!basename) basename = strrchr(file, '\\');
    basename = basename ? basename + 1 : file;

    // Format the user message
    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    // Output
    const char* level_str = LevelToString(level);
    fprintf(stderr, "[%s] [%s] %s:%d: %s\n",
            time_str, level_str, basename, line, msg_buf);

    if (file_) {
        fprintf(file_, "[%s] [%s] %s:%d: %s\n",
                time_str, level_str, basename, line, msg_buf);
        fflush(file_);
    }
}

void Logger::Close() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
    filepath_.clear();
}

}  // namespace utils
}  // namespace rtdiag
