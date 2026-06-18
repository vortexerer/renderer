#pragma once

#include <string>
#include <string_view>
#include <source_location>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <fstream>

enum class LogLevel {
    Info,
    Warning,
    Error,
    Debug
};

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static void Log(std::string_view message, LogLevel level = LogLevel::Info, 
                    std::source_location location = std::source_location::current());

    static void Info(std::string_view message, std::source_location location = std::source_location::current());
    static void Warning(std::string_view message, std::source_location location = std::source_location::current());
    static void Error(std::string_view message, std::source_location location = std::source_location::current());
    static void Debug(std::string_view message, std::source_location location = std::source_location::current());

    void Shutdown();

private:
    struct LogEntry {
        std::string message;
        LogLevel level;
        std::string file;
        std::string function;
        unsigned int line;
        std::string timestamp;
    };

    Logger();
    ~Logger();

    void PushLog(std::string_view message, LogLevel level, const std::source_location& location);
    void ProcessQueue();
    void WriteLog(const LogEntry& entry);

    std::queue<LogEntry> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_CondVar;
    std::thread m_WorkerThread;
    std::ofstream m_LogFile;
    bool m_ExitRequested;
};
