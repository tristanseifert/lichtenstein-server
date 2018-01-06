#include "EffectRunner.h"

#include "DataStore.h"
#include "OutputMapper.h"
#include "Framebuffer.h"
#include "Routine.h"

#include <glog/logging.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>

#include <ctime>

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
	VLOG(1) << "Deleted coordinator";

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

	LOG(INFO) << "Using " << numThreads << " threads for thread pool";

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
	pthread_setname_np("Effect Coordinator");
#else
	pthread_setname_np(pthread_self(), "Effect Coordinator");
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
	// initialize some atomics
	this->frameCounter = 0;
	this->outstandingEffects = 0;
	this->outstandingConversions = 0;

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
	double sleepTimeNs = ((1000 * 1000 * 1000) / double(fps));

	struct timespec sleep;
	sleep.tv_sec = 0;

	// fetch all output channels (TODO: do this if they change too)
	this->outputChannels = this->store->getAllChannels();

	// starting time
	auto start = chrono::high_resolution_clock::now();
	this->fpsStart = chrono::high_resolution_clock::now();

	// run as long as the main thread is still alive
	while(this->coordinatorRunning) {
		// check if we have effects to run
		if(this->mapper->outputMap.empty() == false) {
			// run the effect routines
			if(this->coordinatorRunning == false) return;
			this->coordinatorRunEffects();

			// do the framebuffer conversions
			if(this->coordinatorRunning == false) return;
			this->coordinatorDoConversions();
		}

		// determine how long it took to do all that, sleep for the remainder
		auto end = chrono::high_resolution_clock::now();
		chrono::duration<double, std::nano> difference = (end - start);
		long differenceNanos = difference.count();

		sleep.tv_nsec = (sleepTimeNs - differenceNanos);
		sleep.tv_nsec -= this->sleepInaccuracy;
		nanosleep(&sleep, nullptr);

		// immediately after we wake up, get the new starting time
		start = chrono::high_resolution_clock::now();

		// VLOG(1) << "Slept for " <<  sleep.tv_nsec << " ns";

		// fps accounting
		this->calculateActualFps();

		// how long did we sleep for?
		auto sleepTime = chrono::high_resolution_clock::now() - end;
		chrono::duration<double, std::micro> micros = sleepTime;

		double sleepTimeUs = micros.count();
		double sleepTimeNs = chrono::duration<double, std::nano>(sleepTime).count();

		// calculate the compensation factor
		this->compensateSleepInaccuracies(sleep.tv_nsec, sleepTimeNs);

		// add it to the moving average
		double n = this->avgSleepTimeSamples;
		double oldAvg = this->avgSleepTime;

		double newAvg = ((oldAvg * n) + sleepTimeUs) / (n + 1);

		this->avgSleepTime = newAvg;
		this->avgSleepTimeSamples++;

		LOG_EVERY_N(INFO, 60) << "Avg sleep time: " << this->avgSleepTime << " µS";
		LOG_EVERY_N(INFO, 60) << "Actual fps: " << this->actualFps;
	}

	// cleanup
	LOG(INFO) << "Shutting down coordinator thread";

	for(auto channel : this->outputChannels) {
		delete channel;
	}
	this->outputChannels.clear();
}

/**
 * Calculates the "compensation factor" on the nanosleep() call. This calculates
 * a moving average of the difference between the actual and requested sleep
 * times, and applies that as a correction.
 *
 * TODO: Reset the average every now and then? If the system suddenly becomes
 * heavily loaded, we may not compensate quickly enough.
 */
void EffectRunner::compensateSleepInaccuracies(long requestedNs, long actualNs) {
	// calculate the difference
	double difference = (actualNs - requestedNs);

	// add to average
	double n = this->sleepInaccuracySamples;
	double oldAvg = this->sleepInaccuracy;

	double newAvg = ((oldAvg * n) + difference) / (n + 1);

	this->sleepInaccuracy = newAvg;
	this->sleepInaccuracySamples++;

	// logging
	VLOG_EVERY_N(1, 60) << "nanosleep inaccuracy: " << this->sleepInaccuracy << " ns";
}

/**
 * Calculates the actual FPS that the coordinator is achieving.
 */
void EffectRunner::calculateActualFps() {
	this->actualFramesCounter++;

	auto current = chrono::high_resolution_clock::now();
	chrono::duration<double, std::milli> fpsDifference = (current - this->fpsStart);

	if(fpsDifference.count() >= 1000.f) {
		this->actualFps = double(this->actualFramesCounter) / (fpsDifference.count() / 1000.f);

		// reset the frame counter and timer
		this->actualFramesCounter = 0;
		this->fpsStart = chrono::high_resolution_clock::now();
	}
}

/**
 * Called whenever we actually have effects to run. This will push each output
 * group's routine onto the thread pool, wait for all of them to finish, then
 * convert the framebuffers, and once the conversion of every buffer has
 * completed, output it to the nodes.
 */
void EffectRunner::coordinatorRunEffects() {
	// set up the condition variable
	this->outstandingEffects = this->mapper->outputMap.size();

	// run each effect
	for(auto const& [group, routine] : this->mapper->outputMap) {
		this->workPool->push([this, &group = group, &routine = routine] (int tid) {
			this->runEffect(group, routine);
		});
	}

	// wait for the effects to complete
	{
        unique_lock<mutex> lk(this->effectLock);
        this->effectsCv.wait(lk, [this]{
			return (this->outstandingEffects == 0);
		});
	}

	// allow the devices to output the data

	// advance frame counter
	this->frameCounter++;
}

/**
 * Handles the conversion of each of the effects' outputs. This copies the data
 * from each effect into the output framebuffer, then converts the HSI data in
 * that framebuffer to the format (RGB/RGBW) required by each of the output
 * channels.
 */
void EffectRunner::coordinatorDoConversions() {
	// set up the condition variable
	unsigned int conversions = this->outputChannels.size();
	this->outstandingConversions = conversions;

	// handle the case of having zero configured output channels
	if(conversions == 0) {
		return;
	}

	// perform the copying from framebuffers to channels and conversion
	for(auto channel : this->outputChannels) {
		this->workPool->push([this, &channel = channel] (int tid) {
			this->convertPixelData(channel);
		});
	}

	// wait for the conversions to complete
	{
		unique_lock<mutex> lk(this->effectLock);
		this->conversionCv.wait(lk, [this]{
			return (this->outstandingConversions == 0);
		});
	}
}

/**
 * Runs a single effect.
 */
void EffectRunner::runEffect(OutputMapper::OutputGroup *group, Routine *routine) {
	// do boring effect running stuff
	group->bindBufferToRoutine(routine);
	routine->execute(this->frameCounter);

	// copy the framebuffer data out of the group
	group->copyIntoFramebuffer(this->fb);

	// decrement the outstanding effects
	this->outstandingEffects--;

	// notify the coordinator
    this->effectLock.unlock();
    this->effectsCv.notify_one();
}

/**
 * Converts the pixel data for the given channel. This reads the HSI pixels from
 * the main framebuffer, converts it, and writes it into the buffer for that
 * channel.
 */
void EffectRunner::convertPixelData(DbChannel *channel) {


	// decrement the outstanding conversions
	this->outstandingConversions--;

	// notify the coordinator
	this->effectLock.unlock();
	this->conversionCv.notify_one();
}
