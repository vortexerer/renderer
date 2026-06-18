#include "Logger.h"
#include <iostream>
#include <ctime>

Logger::Logger() {
    m_LogFile.open("Engine.log", std::ios::out | std::ios::app);
    m_ExitRequested = false;
    m_WorkerThread = std::thread(&Logger::ProcessQueue, this);
}

Logger::~Logger() {
    Shutdown();
    if (m_LogFile.is_open()) {
        m_LogFile.close();
    }
}

void Logger::Log(std::string_view message, LogLevel level, std::source_location location) {
    GetInstance().PushLog(message, level, location);
}

void Logger::Info(std::string_view message, std::source_location location) {
    Log(message, LogLevel::Info, location);
}

void Logger::Warning(std::string_view message, std::source_location location) {
    Log(message, LogLevel::Warning, location);
}

void Logger::Error(std::string_view message, std::source_location location) {
    Log(message, LogLevel::Error, location);
}

void Logger::Debug(std::string_view message, std::source_location location) {
    Log(message, LogLevel::Debug, location);
}

void Logger::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_ExitRequested) return;
        m_ExitRequested = true;
    }
    m_CondVar.notify_one();
    if (m_WorkerThread.joinable()) {
        m_WorkerThread.join();
    }
}

void Logger::PushLog(std::string_view message, LogLevel level, const std::source_location& location) {
    char time_str[32];
    time_t rawtime;
    std::time(&rawtime);
    struct tm timeinfo;
#ifdef _WIN32
    localtime_s(&timeinfo, &rawtime);
#else
    localtime_r(&rawtime, &timeinfo);
#endif
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    LogEntry entry {
        std::string(message),
        level,
        location.file_name(),
        location.function_name(),
        location.line(),
        std::string(time_str)
    };

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Queue.push(std::move(entry));
    }
    m_CondVar.notify_one();
}

void Logger::ProcessQueue() {
    while (true) {
        LogEntry entry;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_CondVar.wait(lock, [this] { return !m_Queue.empty() || m_ExitRequested; });

            if (m_Queue.empty() && m_ExitRequested) {
                break;
            }

            entry = std::move(m_Queue.front());
            m_Queue.pop();
        }

        WriteLog(entry);
    }

    // Process remaining logs
    std::lock_guard<std::mutex> lock(m_Mutex);
    while (!m_Queue.empty()) {
        WriteLog(m_Queue.front());
        m_Queue.pop();
    }
}

void Logger::WriteLog(const LogEntry& entry) {
    std::string level_str;
    std::string color_code;
    switch (entry.level) {
        case LogLevel::Info:
            level_str = "INFO";
            color_code = "\033[92m"; // Green
            break;
        case LogLevel::Warning:
            level_str = "WARN";
            color_code = "\033[93m"; // Yellow
            break;
        case LogLevel::Error:
            level_str = "ERROR";
            color_code = "\033[91m"; // Red
            break;
        case LogLevel::Debug:
            level_str = "DEBUG";
            color_code = "\033[96m"; // Cyan
            break;
    }

    std::string_view filepath(entry.file);
    size_t last_slash = filepath.find_last_of("\\/");
    if (last_slash != std::string_view::npos) {
        filepath = filepath.substr(last_slash + 1);
    }

    std::string formatted = entry.timestamp + " [" + level_str + "] [" + std::string(filepath) + ":" + std::to_string(entry.line) + "] (" + entry.function + ") - " + entry.message;

    // Diagnostics go to stderr so that stdout stays a clean data channel
    // (the E2E test runner parses JSON emitted on stdout).
    std::cerr << color_code << formatted << "\033[0m\n";

    if (m_LogFile.is_open()) {
        m_LogFile << formatted << "\n";
        m_LogFile.flush();
    }
}
