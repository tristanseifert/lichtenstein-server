/**
 * Main entrypoint for Lichtenstein server
 */
#include "version.h"

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <iostream>
#include <atomic>

#include <signal.h>

#include "ConfigManager.h"

// when set to false, the server terminates
std::atomic_bool keepRunning;

// define flags
DEFINE_string(config_path, "./lichtenstein.conf", "Path to the server configuration file");
DEFINE_int32(verbosity, 4, "Debug logging verbosity");

/**
 * Signal handler. This handler is invoked for the following signals to enable
 * us to do a clean shut-down:
 *
 * - SIGINT
 */
void signalHandler(int sig) {
    LOG(WARNING) << "Caught signal " << sig << "; shutting down!";
    keepRunning = false;
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
    // set up logging
    FLAGS_logtostderr = 1;
    FLAGS_colorlogtostderr = 1;

    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    LOG(INFO) << "lichtenstein server " << gVERSION_HASH << "/" << gVERSION_BRANCH << " starting up";

    // interpret command-line flags and read config
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    try {
        ConfigManager::readConfig(FLAGS_config_path);
    } catch(ConfigManager::ParseException &e) { 
        LOG(FATAL) << "Parse error on line " << e.getLine() << " of config: "
            << e.what();
    } catch(std::exception &e) {
        LOG(FATAL) << "Failed to read config from '" << FLAGS_config_path 
            << "' (" << e.what() << ")";
    }

    int verbosity = ConfigManager::getNumber("logging.verbosity", 0);
    if (verbosity < 0) {
        FLAGS_v = abs(verbosity);
        FLAGS_minloglevel = 0;
    } else {
        // don't disallow logging of fatal errors
        FLAGS_v = 0;
        FLAGS_minloglevel = std::min(verbosity, 2);
    }

    FLAGS_logtostderr = ConfigManager::getBool("logging.stderr", true) ? 1 : 0;
    FLAGS_colorlogtostderr = ConfigManager::getBool("logging.stderr_color", true) ? 1 : 0;

    // set up a signal handler for termination so we can close down cleanly
    keepRunning = true;

    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, nullptr);

    // wait for a signal
    while(keepRunning) {
        pause();
    }
}
