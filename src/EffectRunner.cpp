#include "EffectRunner.h"

#include "DataStore.h"
#include "OutputMapper.h"
#include "Framebuffer.h"

#include <glog/logging.h>

#include <thread>

using namespace std;

/**
 * Initializes the effect runner, worker threads, and the framebuffer and output
 * mapper.
 */
EffectRunner::EffectRunner(DataStore *store, INIReader *config) {
	this->config = config;
	this->store = store;

	// allocate the framebuffer
	this->fb = new Framebuffer(store, config);
	this->fb->recalculateMinSize();

	// create the output mapper
	this->mapper = new OutputMapper(store, this->fb, config);

	// set up the worker thread pool
	this->setUpThreadPool();

	// set up the coordinator thread
	this->setUpCoordinatorThread();
}

/**
 * Cleans up any resources we allocated and tear down the worker threads.
 */
EffectRunner::~EffectRunner() {
	// get rid of our worker thread pool
	delete this->workPool;

	// Wait for the coordinator to finish
	this->coordinatorRunning = false;
	this->coordinator->join();

	delete this->coordinator;

	// delete framebuffer, mapper
	delete this->fb;
	delete this->mapper;
}

/**
 * Sets up the thread pool.
 */
void EffectRunner::setUpThreadPool() {
	int numThreads = this->config->GetInteger("runner", "maxThreads", 0);

	// if zero, create half as many threads as we have cores
	if(numThreads == 0) {
		numThreads = thread::hardware_concurrency();
		CHECK(numThreads > 0) << "Couldn't get number of cores!";

		numThreads = max((numThreads / 2), 1);
	}

	// set up the thread pool
	this->workPool = new ctpl::thread_pool(numThreads);
	CHECK(this->workPool != nullptr) << "Couldn't allocate worker thread pool";
}

#pragma mark - Coordinator Thread Entry
/**
 * Coordinator thread entry point
 */
void CoordinatorEntryPoint(void *ctx) {
#ifdef __APPLE__
	pthread_setname_np("Command Server");
#else
	pthread_setname_np(pthread_self(), "Command Server");
#endif

	EffectRunner *runner = static_cast<EffectRunner *>(ctx);
	runner->coordinatorThreadEntry();
}

/**
 * Sets up the coordinator thread. This thread essentially runs a high accuracy
 * timer that fires at the specified framerate, and then runs each effect.
 *
 * It also handles the task of waiting for each effect to run to completion,
 * converting the framebuffers and handing that data off to the protocol layer.
 */
void EffectRunner::setUpCoordinatorThread() {
	// allow the thread to run
	this->coordinatorRunning = true;

	// lastly, start the thread
	this->coordinator = new thread(CoordinatorEntryPoint, this);
}

/**
 * Entry point for the coordinator thread.
 */
void EffectRunner::coordinatorThreadEntry() {
	// get some config
	int fps = this->config->GetInteger("runner", "fps", 30);

	LOG(INFO) << "Started coordinator thread, fps = " << fps;

	// set up the timer

	// run as long as the main thread is still alive
	while(this->coordinatorRunning) {

	}
}
