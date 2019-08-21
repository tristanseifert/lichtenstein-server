/**
 * Main entrypoint for Lichtenstein server
 */
#include "version.h"

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <pthread.h>

#include "INIReader.h"

#include <iostream>
#include <atomic>

#include <signal.h>

#include "ProtocolHandler.h"
#include "DataStore.h"
#include "EffectRunner.h"
#include "Routine.h"

// when set to false, the server terminates
std::atomic_bool keepRunning;

// data store
static DataStore *store = nullptr;

// various components of the server
static ProtocolHandler *protocol = nullptr;
static EffectRunner *runner = nullptr;

// define flags
DEFINE_string(config_path, "./lichtenstein.conf", "Path to the server configuration file");
DEFINE_int32(verbosity, 4, "Debug logging verbosity");

// parsing of the config file
INIReader *configReader = nullptr;

void parseConfigFile(const std::string &path);

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

	LOG(INFO) << "lichtenstein server " << gVERSION_HASH << "/" << gVERSION_BRANCH;

	// interpret command-line flags
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	// first, parse the config file
	parseConfigFile(FLAGS_config_path);

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

	// load the datastore from disk
	store = new DataStore(configReader);

	// start the protocol parser (binary lichtenstein protocol)
	protocol = new ProtocolHandler(store, configReader);

	// start the effect evaluator
	runner = new EffectRunner(store, configReader, protocol);

	// XXX: Test the routine code
	/*std::vector<DbRoutine *> routines = store->getAllRoutines();
	DataStore::Routine *r = routines[0];
	LOG(INFO) << "Found routines: " << *r;

	Routine *rout;

	try {
		rout = new Routine(r);
	} catch (Routine::LoadError e) {
		LOG(ERROR) << "Error loading script: " << e.what();
	}

	std::vector<HSIPixel> buf;
	buf.resize(300, {0, 0, 0});
	rout->attachBuffer(&buf);

	for(int i = 0; i < 1000; i++) {
		rout->execute(i);
	}

	LOG(INFO) << "Average script execution time: " << rout->getAvgExecutionTime() << "µS";
	*/

	// XXX: Test the effect evaluation code
/*	std::vector<DbGroup *> groups = store->getAllGroups();
	std::vector<DbRoutine *> routines = store->getAllRoutines();

	auto mapper = runner->getMapper();

	for(int i = 0; i < groups.size(); i++) {
		DbGroup *dbG = groups[i];

		unsigned int routineIndex = i;

		if(routineIndex >= routines.size()) {
			routineIndex = (routines.size() - 1);
		}
		DbRoutine *dbR = routines[routineIndex];

		// create the output group and add the mapping
		OutputMapper::OutputGroup *g = new OutputMapper::OutputGroup(dbG);
		Routine *r = new Routine(dbR);

		mapper->addMapping(g, r);
	}*/

	// wait for a signal
	while(keepRunning) {
		pause();
	}

	// clean up
	delete runner;
	delete protocol;

	// delete the datastore last
	delete store;
}

/**
 * Opens the config file for reading and parses it.
 */
void parseConfigFile(const std::string &path) {
	int err;

	LOG(INFO) << "Reading configuration from " << path;

	// attempt to open the config file
	configReader = new INIReader(path);

	err = configReader->ParseError();

	if(err == -1) {
		LOG(FATAL) << "Couldn't open config file at " << path;
	} else if(err > 0) {
		LOG(FATAL) << "Parse error on line " << err << " of config file " << path;
	}

	// set up the logging parameters
	int verbosity = configReader->GetInteger("logging", "verbosity", 0);

	if(verbosity < 0) {
		FLAGS_v = abs(verbosity);
		FLAGS_minloglevel = 0;

		LOG(INFO) << "Enabled verbose logging up to level " << abs(verbosity);
	} else {
		// disable verbose logging
		FLAGS_v = 0;

		// ALWAYS log FATAL errors
		FLAGS_minloglevel = std::min(verbosity, 2);
	}

	FLAGS_logtostderr = configReader->GetBoolean("logging", "stderr", true);
}
