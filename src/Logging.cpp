#include "Logging.h"

#include <iostream>
#include <memory>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>

#include "ConfigManager.h"

using namespace Lichtenstein::Server;

// shared instance of the logging handler
std::unique_ptr<Logging> shared = nullptr;

/**
 * When logging starts, create the shared logging handler.
 */
void Logging::start() {
    shared = std::make_unique<Logging>();
}

/**
 * When logging is desired to be stopped, delete the shared logging handler.
 */
void Logging::stop() {
    shared = nullptr;
}

/**
 * Configure spdlog to log to stdout, file, and/or syslog as configured.
 */
Logging::Logging() {
    using spdlog::logger, spdlog::sink_ptr, std::make_shared, std::vector;

    vector<sink_ptr> sinks;

    // do we want logging to the console?
    if(ConfigManager::getBool("logging.console.enabled", true)) {
        this->configTtyLog(sinks);
    }
    // do we want to log to a file?
    if(ConfigManager::getBool("logging.file.enabled", false)) {
        this->configFileLog(sinks);
    }
    // do we want to log to syslog?
    if(ConfigManager::getBool("logging.syslog.enabled", false)) {
        this->configSyslog(sinks);
    }

    // Warn if no loggers configured
    if(sinks.empty()) {
        std::cerr << "WARNING: No logging sinks configured" << std::endl;
    }

    // create a multi-logger for this
    this->logger = make_shared<logger>("", sinks.begin(), sinks.end());
    spdlog::set_default_logger(this->logger);
}
/**
 * Cleans up logging.
 */
Logging::~Logging() {
    spdlog::shutdown();
}



/**
 * Configures the console logger.
 */
void Logging::configTtyLog(std::vector<spdlog::sink_ptr> &sinks) {
    auto level = this->getLogLevel("logging.console.level", 2);

    // Do we want a colorized logger?
    if(ConfigManager::getBool("logging.console.colorize", false)) {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sink->set_level(level);
        sinks.push_back(sink);
    } 
    // Plain logger
    else {
        auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        sink->set_level(level);
        sinks.push_back(sink);
    }
}

/**
 * Configures the file logger.
 */
void Logging::configFileLog(std::vector<spdlog::sink_ptr> &sinks) {
    using spdlog::sinks::basic_file_sink_mt;

    // get the file logger params
    auto level = this->getLogLevel("logging.file.level", 2);
    std::string path = ConfigManager::get("logging.file.path", "");
    bool truncate = ConfigManager::getBool("logging.file.truncate", false);

    // create the file logger
    auto file = std::make_shared<basic_file_sink_mt>(path, truncate);
    file->set_level(level);

    sinks.push_back(file);
}

/**
 * Configures the syslog logger.
 */
void Logging::configSyslog(std::vector<spdlog::sink_ptr> &sinks) {
    // TODO: implement
}

/**
 * Converts a numeric log level from the config file into the spdlog value.
 */
spdlog::level::level_enum Logging::getLogLevel(const std::string &path, unsigned long def) {
    static const spdlog::level::level_enum array[] = {
        spdlog::level::trace,
        spdlog::level::debug,
        spdlog::level::info,
        spdlog::level::warn,
        spdlog::level::err,
        spdlog::level::critical
    };

    // read the preference
    unsigned long level = ConfigManager::getUnsigned(path, def);

    // ensure it's in bounds then check the array
    if(level >= 6) {
        return spdlog::level::trace;
    } else {
        return array[level];
    }
}
