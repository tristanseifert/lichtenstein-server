/**
 * Main entrypoint for Lichtenstein server
 */
#include "version.h"

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <pthread.h>

#include <libconfig.h++>

#include <iostream>
#include <atomic>

#include <signal.h>

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

	// set thread name
	#ifdef __APPLE__
		pthread_setname_np("Main Thread");
	#else
    #ifdef pthread_setname_np
		  pthread_setname_np(pthread_self(), "Main Thread");
    #endif
	#endif

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
