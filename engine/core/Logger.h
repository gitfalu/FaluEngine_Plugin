#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/dist_sink.h>
#include <memory>
#include <filesystem>

namespace FaluEngine {

class Logger {
public:
    static void init(const std::filesystem::path& logDir = "logs");
    static void shutdown();

    [[nodiscard]] static std::shared_ptr<spdlog::logger>& getEngineLogger() { return s_engineLogger; }
    [[nodiscard]] static std::shared_ptr<spdlog::logger>& getGameLogger() { return s_gameLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_engineLogger;
    static std::shared_ptr<spdlog::logger> s_gameLogger;

private:
    static std::shared_ptr<spdlog::logger> createLogger(
        const std::string& name,
        const std::filesystem::path& logFile,
        spdlog::color_mode colorMode = spdlog::color_mode::always
    );
};

} // namespace FaluEngine

// 便利マクロ
//======= エンジン内部マクロ ==========
#define LOG_TRACE(...)    ::FaluEngine::Logger::getGameLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)     ::FaluEngine::Logger::getGameLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::FaluEngine::Logger::getGameLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::FaluEngine::Logger::getGameLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::FaluEngine::Logger::getGameLogger()->critical(__VA_ARGS__)

//======= ゲーム側ログマクロ ==========
#define FALU_ENGINE_LOG_TRACE(...)    ::FaluEngine::Logger::getEngineLogger()->trace(__VA_ARGS__)
#define FALU_ENGINE_LOG_INFO(...)     ::FaluEngine::Logger::getEngineLogger()->info(__VA_ARGS__)
#define FALU_ENGINE_LOG_WARN(...)     ::FaluEngine::Logger::getEngineLogger()->warn(__VA_ARGS__)
#define FALU_ENGINE_LOG_ERROR(...)    ::FaluEngine::Logger::getEngineLogger()->error(__VA_ARGS__)
#define FALU_ENGINE_LOG_CRITICAL(...) ::FaluEngine::Logger::getEngineLogger()->critical(__VA_ARGS__)



