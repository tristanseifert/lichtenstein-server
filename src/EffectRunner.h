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

#include "INIReader.h"
#include "CTPL/ctpl.h"

#include <thread>
#include <atomic>

class DataStore;
class OutputMapper;
class Framebuffer;

class EffectRunner {
	public:
		EffectRunner(DataStore *store, INIReader *config);
		~EffectRunner();

	private:
		void setUpThreadPool();

	private:
		friend void CoordinatorEntryPoint(void *ctx);

		void setUpCoordinatorThread();
		void coordinatorThreadEntry();

		std::thread *coordinator;
		std::atomic_bool coordinatorRunning;

	private:
		DataStore *store;
		INIReader *config;

		Framebuffer *fb;
		OutputMapper *mapper;

		ctpl::thread_pool *workPool;
};

#endif
