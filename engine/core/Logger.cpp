#include "Logger.h"

namespace FaluEngine {

std::shared_ptr<spdlog::logger> Logger::s_engineLogger;
std::shared_ptr<spdlog::logger> Logger::s_gameLogger;

void Logger::init(const std::filesystem::path& logDir) {
    // logs/ ディレクトリが無ければ作成
    std::filesystem::create_directories(logDir);

    s_engineLogger = createLogger("Engine", logDir / "engine.log");
    s_gameLogger = createLogger("Game", logDir / "game.log");

    LOG_INFO("Logger initialized - engine.log / game.log -> {}", logDir.string());
}

void Logger::shutdown()
{
    spdlog::shutdown();
}

std::shared_ptr<spdlog::logger> Logger::createLogger(
    const std::string& name, 
    const std::filesystem::path& logFile, 
    spdlog::color_mode colorMode)
{
    // コンソールシンク(色付き)
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>(colorMode);
    consoleSink->set_pattern("%^[%H:%M:%S] [%n] [%l]%$ %v");

    // ファイルシンク(色なし・追記モード)
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        logFile.string(), false
    );
    fileSink->set_pattern("[%Y-%m-%d %H:%M:%S] [%n] [%l] %v");

    // 両方に同時出力する
    auto distSink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    distSink->add_sink(consoleSink);
    distSink->add_sink(fileSink);

    auto logger = std::make_shared<spdlog::logger>(name, distSink);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(logger);

    return logger;
}

} // namespace FaluEngine
