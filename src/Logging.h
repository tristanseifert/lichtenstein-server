/**
 * Central logger for the rest of the application. Automagically handles
 * sending messages to the correct outputs.
 */
#ifndef LOGGING_H
#define LOGGING_H

#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace Lichtenstein::Server {
    class Logging {
        public:
            static void start();
            static void stop();

            // you shouldn't call this lol
            Logging();
            virtual ~Logging();

        public:
            /// Trace level logging
            template<typename... Args> static inline void trace(std::string fmt, 
                    const Args &... args) {
                spdlog::trace(fmt, args...);
            }
            
            /// Debug level logging
            template<typename... Args> static inline void debug(std::string fmt, 
                    const Args &... args) {
                spdlog::debug(fmt, args...);
            }
            
            /// Info level logging
            template<typename... Args> static inline void info(std::string fmt, 
                    const Args &... args) {
                spdlog::info(fmt, args...);
            }
            
            /// Warning level logging
            template<typename... Args> static inline void warn(std::string fmt, 
                    const Args &... args) {
                spdlog::warn(fmt, args...);
            }
            
            /// Error level logging
            template<typename... Args> static inline void error(std::string fmt, 
                    const Args &... args) {
                spdlog::error(fmt, args...);
            }
            
            /// Critical level logging
            template<typename... Args> static inline void crit(std::string fmt, 
                    const Args &... args) {
                spdlog::critical(fmt, args...);
            }

        private:
            void configTtyLog(std::vector<spdlog::sink_ptr> &);
            void configFileLog(std::vector<spdlog::sink_ptr> &);
            void configSyslog(std::vector<spdlog::sink_ptr> &);

            static spdlog::level::level_enum getLogLevel(const std::string &, unsigned long);
            static int getSyslogFacility(const std::string &path, int);

        private:
            std::shared_ptr<spdlog::logger> logger;
    };
}

#endif
