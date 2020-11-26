/**
 * Main entrypoint for Lichtenstein server
 */
#include "version.h"

#include <atomic>
#include <iostream>
#include <sstream>

#include <unistd.h>
#include <signal.h>

#include <cxxopts.hpp>

#include <ConfigManager.h>
#include <Logging.h>

#include "db/DataStore.h"
#include "api/Server.h"
#include "render/Pipeline.h"
#include "proto/Server.h"
#include "proto/Syncer.h"

// bring all our namespaces into scope
using namespace Lichtenstein::Server;

// when set to false, the server terminates
static std::atomic_bool keepRunning;

// command line configuration
static struct {
    // config file path
    std::string cfgPath = "./lichtenstein.conf";
} cmdline;

/**
 * Signal handler. This handler is invoked for the following signals to enable
 * us to do a clean shut-down:
 *
 * - SIGINT
 */
static void signalHandler(int sig) {
    Logging::warn("Caught signal {}; shutting down!", sig);
    keepRunning = false;

    // mask the signal now
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);

    sigprocmask(SIG_BLOCK, &set, nullptr);
}

/**
 * Parses the command line. If false is returned, the program should not
 * continue further.
 */
static bool ParseCmdLine(int argc, char *argv[]) {
    using namespace cxxopts;

    Options opt("lichtenstein_server", "Lichtenstein effects server");

    opt.add_options()
        ("c,config", "Path to config file")
        ("version", "Print program version and exit")
        ("h,help", "Print usage and exit")
    ;

    // parse the options
    try {
        auto result = opt.parse(argc, argv);

        if(result.count("config")) {
            cmdline.cfgPath = result["config"].as<std::string>();
        }

        // print help?
        if(result.count("help")) {
            std::cout << opt.help() << std::endl;
            return false;
        }
        // print version?
        else if(result.count("version")) {
            std::cout << "lichtenstein_server " << gVERSION << " (" 
                << std::string(gVERSION_HASH).substr(0, 8) << ")" << std::endl;
            return false;
        }
    } catch(option_not_exists_exception &e) {
        std::cerr << "Failed to parse command line: " << e.what() << std::endl;
        return false;
    }

    // we good
    return true;
}

/**
 * Reads the config file and configures early settings. Returns false if an
 * error took place, true otherwise.
 */
static bool LoadConfig(void) {
    // try to read the config file
    try {
        ConfigManager::readConfig(cmdline.cfgPath);
    } catch(ConfigManager::ParseException &e) { 
        std::cerr << "Parse error on line " << e.getLine() << " of config: "
            << e.what() << std::endl;
        return false;
    } catch(std::exception &e) {
        std::cerr << "Failed to read config from '" << cmdline.cfgPath
            << "' (" << e.what() << ")" << std::endl;
        return false;
    }

    return true;
}

/**
 * Starts all required services. Any exceptions during this process will result
 * in an immediate termination.
 */
void StartServices() {
    try {
        DB::DataStore::open();
        Render::Pipeline::start();
        Proto::Server::start();
        Proto::Syncer::start();
        API::Server::start();
    } catch(std::exception &e) {
        Logging::crit("StartServices() failed: {}", e.what());
        Logging::stop();

        std::abort();
    }
}

/**
 * Stops all services that were started earlier.
 */
void StopServices() {
    try {
        API::Server::stop();
        Render::Pipeline::stop();
        Proto::Syncer::stop();
        Proto::Server::stop();
        DB::DataStore::close();
    } catch(std::exception &e) {
        Logging::crit("StopServices() failed: {}", e.what());
        Logging::stop();

        std::abort();
    }

    Logging::info("All services stopped");
}



/**
 * Main function
 */
int main(int argc, char *argv[]) {
    // parse command line, load config and set up logging
    if(!ParseCmdLine(argc, argv)) {
        return 0;
    }
    if(!LoadConfig()) {
        return -1;
    }

    Logging::start();

    // set up a signal handler for termination so we can close down cleanly
    keepRunning = true;

    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, nullptr);

    // start all required services
    StartServices();

    // wait for a signal
    Logging::info("lichtenstein_server is ready");

    while(keepRunning) {
        ::pause();
    }

    // shut down all allocated services
    StopServices();
    Logging::stop();
}

