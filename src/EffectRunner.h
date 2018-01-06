/**
 * Implements the facilities for actually running effects. This holds the output
 * mapper that maps effects to output groups, runs them at the correct timing,
 * then converts between HSI and RGB(W) for each output channel.
 *
 * Data is then packaged and sent to each node.
 */
#ifndef EFFECTRUNNER_H
#define EFFECTRUNNER_H

#include "HSIPixel.h"
#include "OutputMapper.h"

#include "INIReader.h"
#include "CTPL/ctpl.h"

#include <thread>
#include <atomic>
#include <condition_variable>

class DataStore;
class Framebuffer;
class DbChannel;
class Routine;

class EffectRunner {
	public:
		EffectRunner(DataStore *store, INIReader *config);
		~EffectRunner();

	public:
		inline OutputMapper *getMapper() const {
			return this->mapper;
		}

	private:
		void setUpThreadPool();

	private:
		friend void CoordinatorEntryPoint(void *ctx);

		void setUpCoordinatorThread();

		void coordinatorThreadEntry();
		void coordinatorRunEffects();
		void coordinatorDoConversions();

		void runEffect(OutputMapper::OutputGroup *group, Routine *routine);
		void convertPixelData(DbChannel *channel);

		std::thread *coordinator;
		std::atomic_bool coordinatorRunning;

	// nanosleep inaccuracy compensation
	private:
		double sleepInaccuracy = 0;
		double sleepInaccuracySamples = 0;

		void compensateSleepInaccuracies(long requestedNs, long actualNs);

	// fps accounting
	private:
		double actualFps = 0;
		int actualFramesCounter = 0;
		std::chrono::time_point<std::chrono::high_resolution_clock> fpsStart;

		void calculateActualFps();

	public:
		/// returns the actual frames per second
		double getActualFps() const {
			return this->actualFps;
		}

	// channel handling
	public:
		void updateChannelBuffers();

		std::atomic_bool channelBuffersNeedUpdate;

		std::vector<DbChannel *> outputChannels;
		std::map<DbChannel *, uint8_t *> channelBuffers;

		std::mutex channelBufferMutex;

	private:
		double avgSleepTime = 0;
		double avgSleepTimeSamples = 0;

		std::atomic_int frameCounter;

		std::mutex effectLock;

		std::condition_variable effectsCv;
		std::atomic_uint outstandingEffects;

		std::condition_variable conversionCv;
		std::atomic_uint outstandingConversions;

	private:
		DataStore *store;
		INIReader *config;

		Framebuffer *fb;
		OutputMapper *mapper;

		ctpl::thread_pool *workPool;
};

#endif
