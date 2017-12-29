/**
 * Main entrypoint for Lichtenstein server
 */

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <iostream>
#include <atomic>

#include <signal.h>

#include "CommandServer.h"
#include "NodeDiscovery.h"
#include "DataStore.h"

using namespace std;

// when set to false, the server terminates
atomic_bool keepRunning;

// data store
static DataStore *store = nullptr;

// various components of the server
static CommandServer *cs = nullptr;
static NodeDiscovery *nd = nullptr;

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

	// load the datastore from disk
	store = new DataStore("db/lichtenstein.sqlite3");

	// start the external command interpreter (JSON socket)
	cs = new CommandServer("./lichtenstein.sock", store);
	cs->start();

	// star the node discovery
	nd = new NodeDiscovery(store);
	nd->start();

	// start the protocol parser (binary lichtenstein protocol)

	// start the effect evaluator

	// wait for a signal
	while(keepRunning) {
		pause();
	}

	// stop the servers
	cs->stop();
	nd->stop();

	// clean up
	delete cs;
	delete nd;

	// ensure the database is commited to disk
	store->commit();
	delete store;
}
