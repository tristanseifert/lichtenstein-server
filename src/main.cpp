/**
 * Main entrypoint for Lichtenstein server
 */

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <iostream>
#include <atomic>

#include <signal.h>

#include "CommandServer.h"

using namespace std;

// when set to false, the server terminates
atomic_bool keepRunning;

// various components of the server
static CommandServer *cs = nullptr;

/**
 * Signal handler. This handler is invoked for the following signals to enable
 * us to do a clean shut-down:
 *
 * -
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

	google::InitGoogleLogging(argv[0]);
	google::InstallFailureSignalHandler();

	LOG(INFO) << "lichtenstein server " << GIT_HASH << "/" << GIT_BRANCH
			  << " compiled on " << COMPILE_TIME;

	// set up a signal handler for termination so we can close down cleanly
	keepRunning = true;

	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = signalHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, nullptr);

	// start the external command interpreter (JSON socket)
	CommandServer *cs = new CommandServer("./lichtenstein.sock");
	cs->start();

	// start the protocol parser (binary lichtenstein protocol)

	// start the effect evaluator

	// wait for a signal
	while(keepRunning) {
		pause();
	}

	// stop the servers
	cs->stop();

	// clean up
	delete cs;
}
