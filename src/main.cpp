/**
 * Main entrypoint for Lichtenstein server
 */
#include <glog/logging.h>
#include <gflags/gflags.h>

#include "INIReader.h"

#include <iostream>
#include <atomic>

#include <signal.h>

#include "CommandServer.h"
#include "NodeDiscovery.h"
#include "DataStore.h"
#include "Framebuffer.h"
#include "OutputMapper.h"

using namespace std;

// when set to false, the server terminates
atomic_bool keepRunning;

// data store
static DataStore *store = nullptr;

// various components of the server
static CommandServer *cs = nullptr;
static NodeDiscovery *nd = nullptr;

static Framebuffer *fb = nullptr;
static OutputMapper *mapper = nullptr;

// define flags
DEFINE_string(config_path, "./lichtenstein.conf", "Path to the server configuration file");
DEFINE_int32(verbosity, 4, "Debug logging verbosity");

// parameters read from the config file
string dbPath;
string commandSocketPath;

INIReader *configReader = nullptr;
void parseConfigFile(string path);

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

	LOG(INFO) << "lichtenstein server " << GIT_HASH << "/" << GIT_BRANCH
			  << " compiled on " << COMPILE_TIME;

	// interpret command-line flags
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	FLAGS_v = FLAGS_verbosity;

	// first, parse the config file
	parseConfigFile(FLAGS_config_path);

	// set up a signal handler for termination so we can close down cleanly
	keepRunning = true;

	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = signalHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, nullptr);

	// load the datastore from disk
	store = new DataStore(dbPath);

	// start the external command interpreter (JSON socket)
	cs = new CommandServer(commandSocketPath, store);
	cs->start();

	// star the node discovery
	nd = new NodeDiscovery(store);
	nd->start();

	// allocate the framebuffer
	fb = new Framebuffer(store);
	fb->recalculateMinSize();

	// create the output mapper
	mapper = new OutputMapper(store, fb);

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

	delete mapper;
	delete fb;

	// ensure the database is commited to disk
	store->commit();
	delete store;
}

/**
 * Opens the config file for reading and parses it.
 */
void parseConfigFile(string path) {
	int err;

	// attempt to open the config file
	configReader = new INIReader(path);

	err = configReader->ParseError();

	if(err == -1) {
		LOG(FATAL) << "Couldn't open config file at " << path;
	} else if(err > 0) {
		LOG(FATAL) << "Parse error on line " << err << " of config file " << path;
	}

	// otherwise, get the properties we need to start up
	dbPath = configReader->Get("db", "path", "");
	commandSocketPath = configReader->Get("command", "socket", "");
}
