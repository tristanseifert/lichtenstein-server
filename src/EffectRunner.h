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

#include "CTPL/ctpl.h"

#include <thread>
#include <atomic>
#include <condition_variable>
#include <memory>

class DataStore;
class Framebuffer;
class DbChannel;
class Routine;

namespace protocol {
  class ProtocolHandler;
}

class EffectRunner {
	public:
    EffectRunner(std::shared_ptr<DataStore> store,
                 std::shared_ptr<protocol::ProtocolHandler> proto);
		~EffectRunner();

	public:
    inline std::shared_ptr<OutputMapper> getMapper() const {
			return this->mapper;
		}

	private:
    void setUpThreadPool();

	private:
    void setUpCoordinatorThread();

    void coordinatorThreadEntry();

		std::thread *coordinator;
		std::atomic_bool coordinatorRunning;

		double avgSleepTime = 0;
		double avgSleepTimeSamples = 0;

		std::atomic_int frameCounter;

		std::mutex effectLock;

	// effect running
	private:
    void coordinatorRunEffects();
		void runEffect(OutputMapper::OutputGroup *group, Routine *routine);

		std::condition_variable effectsCv;
		std::atomic_int outstandingEffects;

	// pixel conversion
	private:
    void coordinatorDoConversions();
		void convertPixelData(DbChannel *channel);

		void _convertToRgb(DbChannel *channel);
		void _convertToRgbw(DbChannel *channel);

		std::condition_variable conversionCv;
		std::atomic_int outstandingConversions;

	// data sending
	private:
    void coordinatorSendData();

		void outputPixelData(DbChannel *channel);

		std::condition_variable sendingCv;
		std::atomic_int outstandingSends;

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
    void updateChannels();

    void deleteChannelBuffers();

		std::atomic_bool channelUpdatePending;

		std::vector<DbChannel *> outputChannels;

    std::map<DbChannel *, uint8_t *> channelBuffers;
		std::map<DbChannel *, uint8_t *> channelBuffersPrevFrame;

		std::mutex channelBufferMutex;

	private:
    std::shared_ptr<DataStore> store;

    std::shared_ptr<Framebuffer> fb;
    std::shared_ptr<OutputMapper> mapper;
    std::shared_ptr<protocol::ProtocolHandler> proto;

		ctpl::thread_pool *workPool;
};

#endif
