#include "EffectRunner.h"

#include "ProtocolHandler.h"
#include "DataStore.h"
#include "OutputMapper.h"
#include "Framebuffer.h"
#include "Routine.h"

#include "HSIPixel.h"

#include <glog/logging.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>

#include <ctime>

// log FPS counters
#define LOG_FPS							0

/**
 * Initializes the effect runner, worker threads, and the framebuffer and output
 * mapper.
 */
EffectRunner::EffectRunner(std::shared_ptr<DataStore> store,
                           std::shared_ptr<INIReader> config,
                           std::shared_ptr<ProtocolHandler> proto) : store(
        store), config(config), proto(proto) {
	// allocate the framebuffer
  this->fb = std::make_shared<Framebuffer>(store, config);
	this->fb->recalculateMinSize();

	// create the output mapper
  this->mapper = std::make_shared<OutputMapper>(store, this->fb, config);

	// set up the worker thread pool
	this->setUpThreadPool();

	// set up the coordinator thread
	this->setUpCoordinatorThread();
}

/**
 * Cleans up any resources we allocated and tear down the worker threads.
 */
EffectRunner::~EffectRunner(void) {
	VLOG(1) << "Deallocating effect runner: stopping thread pool";

	// stop the worker thread pool, but don't execute the rest of the queue
	this->workPool->stop(false);

	/*
	 * If the server is terminated while there are still outstanding conversions
	 * or effects, but before we've pushed them all, we could deadlock because
	 * the coordinator expects them all to complete. So, trick the coordinator
	 * into thinking that all conversions are done, let it do its thing (which
	 * would perhaps be some additional conversions we'll throw away) and then
	 * it'll return and exit by itself.
	 *
	 * This lets us save some complexity by avoiding additional locking around
	 * pushing functions onto the worker pool and checking whether the pool has
	 * been deallocated.
	 *
	 * It _would_ be possible to do this with signals, but then we'd have to add
	 * a signal handler and complicate the code some more.
	 */
	this->coordinatorRunning = false;

	this->outstandingConversions = 0;
	this->outstandingEffects = 0;

	this->effectLock.unlock();
	this->effectsCv.notify_one();

  this->effectLock.unlock();
  this->conversionCv.notify_one();

	// signal the output handler
	this->proto->prepareForShutDown();

	// Wait for the coordinator to finish
	VLOG(1) << "Waiting for coordinator to terminate";
	this->coordinator->join();

	delete this->coordinator;
	VLOG(1) << "Deleted coordinator";

	// get rid of our worker thread pool.
	delete this->workPool;

	// delete framebuffer, mapper
  this->mapper = nullptr;
  this->fb = nullptr;

	// deallocate buffers and the channels
	for(auto channel : this->outputChannels) {
		delete channel;
	}

	// delete all buffers
	this->deleteChannelBuffers();
}

/**
 * Sets up the thread pool.
 */
void EffectRunner::setUpThreadPool(void) {
	int numThreads = this->config->GetInteger("runner", "maxThreads", 0);

	// if zero, create half as many threads as we have cores
	if(numThreads == 0) {
		numThreads = std::thread::hardware_concurrency();
		CHECK(numThreads > 0) << "Couldn't get number of cores!";

		numThreads = std::max((numThreads / 2), 1);
	}

	LOG(INFO) << "Using " << numThreads << " threads for thread pool";

	// set up the thread pool
	this->workPool = new ctpl::thread_pool(numThreads);
	CHECK(this->workPool != nullptr) << "Couldn't allocate worker thread pool";
}

#pragma mark - Coordinator Thread Entry
/**
 * Sets up the coordinator thread. This thread essentially runs a high accuracy
 * timer that fires at the specified framerate, and then runs each effect.
 *
 * It also handles the task of waiting for each effect to run to completion,
 * converting the framebuffers and handing that data off to the protocol layer.
 */
void EffectRunner::setUpCoordinatorThread(void) {
	// initialize some atomics
	this->frameCounter = 0;
	this->outstandingEffects = 0;
	this->outstandingConversions = 0;

	// allow the thread to run
	this->coordinatorRunning = true;

	// lastly, start the thread
  this->coordinator = new std::thread(&EffectRunner::coordinatorThreadEntry,
                                      this);
}

/**
 * Entry point for the coordinator thread.
 */
void EffectRunner::coordinatorThreadEntry(void) {
	// get some config
	int fps = this->config->GetInteger("runner", "fps", 30);

	LOG(INFO) << "Started coordinator thread, fps = " << fps;

	// set up the timer
	double sleepTimeNs = ((1000 * 1000 * 1000) / double(fps));

	struct timespec sleep;
	sleep.tv_sec = 0;

	// fetch all output channels and set up buffers
	this->updateChannels();

	// starting time
	auto start = std::chrono::high_resolution_clock::now();
	this->fpsStart = std::chrono::high_resolution_clock::now();

	// run as long as the main thread is still alive
	while(this->coordinatorRunning) {
		// shall we update the channel buffers?
		if(this->channelUpdatePending == true) {
			this->updateChannels();
		}

		// check if we have effects to run
		if(this->mapper->outputMap.empty() == false) {
			// run the effect routines
			if(this->coordinatorRunning == false) goto cleanup;
			this->coordinatorRunEffects();

			// acquire the buffer lock (so they don't get modified)
      std::unique_lock<std::mutex> lk(this->channelBufferMutex);

			// do the framebuffer conversions
			if(this->coordinatorRunning == false) goto cleanup;
			this->coordinatorDoConversions();

			// send pixel data
			if(this->coordinatorRunning == false) goto cleanup;
			this->coordinatorSendData();

			// explicitly unlock it (good practice; it'll get unlocked in the dtor)
			lk.unlock();
		}

		// determine how long it took to do all that, sleep for the remainder
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::nano> difference = (end - start);
		long differenceNanos = difference.count();

		sleep.tv_nsec = (sleepTimeNs - differenceNanos);
		sleep.tv_nsec -= this->sleepInaccuracy;
		nanosleep(&sleep, nullptr);

		// immediately after we wake up, get the new starting time
		start = std::chrono::high_resolution_clock::now();

		// VLOG(1) << "Slept for " <<  sleep.tv_nsec << " ns";

		// fps accounting
		this->calculateActualFps();

		// how long did we sleep for?
		auto sleepTime = std::chrono::high_resolution_clock::now() - end;
		std::chrono::duration<double, std::micro> micros = sleepTime;

		double sleepTimeUs = micros.count();
		double sleepTimeNs = std::chrono::duration<double, std::nano>(sleepTime).count();

		// calculate the compensation factor
		this->compensateSleepInaccuracies(sleep.tv_nsec, sleepTimeNs);

		// add it to the moving average
		double n = this->avgSleepTimeSamples;
		double oldAvg = this->avgSleepTime;

		double newAvg = ((oldAvg * n) + sleepTimeUs) / (n + 1);

		this->avgSleepTime = newAvg;
		this->avgSleepTimeSamples++;

#if LOG_FPS
		LOG_EVERY_N(INFO, 60) << "Avg sleep time: " << this->avgSleepTime << " µS";
		LOG_EVERY_N(INFO, 60) << "Actual fps: " << this->actualFps;
#endif
	}

	cleanup: ;

	// cleanup
	LOG(INFO) << "Shutting down coordinator thread";

	// delete all channels
	for(auto channel : this->outputChannels) {
		delete channel;
	}
	this->outputChannels.clear();

	// delete all buffers
	this->deleteChannelBuffers();
}

/**
 * Fetches all channels and allocates buffers for them.
 */
void EffectRunner::updateChannels(void) {
	// attempt to acquire the lock
  std::unique_lock<std::mutex> lk(this->channelBufferMutex);

	// delete all buffers
	this->deleteChannelBuffers();

	// delete all old channels
	for(auto channel : this->outputChannels) {
		delete channel;
	}

	// fetch all output channels (TODO: do this if they change too)
	this->outputChannels = this->store->getAllChannels();

	// allocate buffers
	for(auto channel : this->outputChannels) {
		size_t numBytes = channel->numPixels;

		// multiply the number of pixels by the number of bytes per pixel
		switch(channel->format) {
			case DbChannel::kPixelFormatRGB:
				numBytes = numBytes * 3;
				break;

			case DbChannel::kPixelFormatRGBW:
				numBytes = numBytes * 4;
				break;
		}

		// allocate the buffer for frame to output
		uint8_t *buf = new uint8_t[numBytes];
		this->channelBuffers[channel] = buf;

		// allocate the buffer for the previous frame (used for delta updates)
		uint8_t *prevFrameBuf = new uint8_t[numBytes];
		this->channelBuffersPrevFrame[channel] = prevFrameBuf;
	}

	// reset the update flag
	this->channelUpdatePending = false;

	// unlock the lock
	lk.unlock();
}
/**
 * Deallocates the buffers for ALL channel buffers.
 */
void EffectRunner::deleteChannelBuffers(void) {
  // delete output buffers
  for(auto const& [channel, buffer] : this->channelBuffers) {
		delete[] buffer;
	}

	this->channelBuffers.clear();

  // delete prev frame buffers
	for(auto const& [channel, buffer] : this->channelBuffersPrevFrame) {
		delete[] buffer;
	}

	this->channelBuffersPrevFrame.clear();
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
#if LOG_FPS
	VLOG_EVERY_N(1, 60) << "nanosleep inaccuracy: " << this->sleepInaccuracy << " ns";
#endif
}

/**
 * Calculates the actual FPS that the coordinator is achieving.
 */
void EffectRunner::calculateActualFps(void) {
	this->actualFramesCounter++;

	auto current = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> fpsDifference = (current - this->fpsStart);

	if(fpsDifference.count() >= 1000.f) {
		this->actualFps = double(this->actualFramesCounter) / (fpsDifference.count() / 1000.f);

		// reset the frame counter and timer
		this->actualFramesCounter = 0;
		this->fpsStart = std::chrono::high_resolution_clock::now();
	}
}



/**
 * Called whenever we actually have effects to run. This will push each output
 * group's routine onto the thread pool, wait for all of them to finish, then
 * convert the framebuffers, and once the conversion of every buffer has
 * completed, output it to the nodes.
 */
void EffectRunner::coordinatorRunEffects(void) {
	// set up the condition variable
	this->outstandingEffects = this->mapper->outputMap.size();

	// run each effect
	this->mapper->outputMapLock.lock();

	for(auto const& [group, routine] : this->mapper->outputMap) {
		// this->workPool->push([this, &group = group, &routine = routine] (int tid) {
			// VLOG_EVERY_N(2, 60) << "Executing routine " << *routine << " with group " << group;
			this->runEffect(group, routine);
		// });
	}

	this->mapper->outputMapLock.unlock();

	// wait for the effects to complete
/*	{
        unique_lock<mutex> lk(this->effectLock);
        this->effectsCv.wait(lk, [this]{
			return (this->outstandingEffects <= 0);
		});
	}
*/

	// advance frame counter
	this->frameCounter++;
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
 * Handles the conversion of each of the effects' outputs. This copies the data
 * from each effect into the output framebuffer, then converts the HSI data in
 * that framebuffer to the format (RGB/RGBW) required by each of the output
 * channels.
 */
void EffectRunner::coordinatorDoConversions(void) {
	// set up the condition variable
	unsigned int conversions = this->outputChannels.size();
	this->outstandingConversions = conversions;

	// handle the case of having zero configured output channels
	if(conversions == 0) {
		return;
	}

	// perform the copying from framebuffers to channels and conversion
	for(auto channel : this->outputChannels) {
		// this->workPool->push([this, &channel = channel] (int tid) {
			this->convertPixelData(channel);
		// });
	}

/*	// wait for the conversions to complete
	{
		unique_lock<mutex> lk(this->effectLock);
		this->conversionCv.wait(lk, [this]{
			return (this->outstandingConversions <= 0);
		});
	}
*/
}

/**
 * Converts the pixel data for the given channel. This reads the HSI pixels from
 * the main framebuffer, converts it, and writes it into the buffer for that
 * channel.
 */
void EffectRunner::convertPixelData(DbChannel *channel) {
	// actually do the conversion lmao
	switch(channel->format) {
		case DbChannel::kPixelFormatRGB:
			this->_convertToRgb(channel);
			break;

		case DbChannel::kPixelFormatRGBW:
			this->_convertToRgbw(channel);
			break;
	}

	// decrement the outstanding conversions
	this->outstandingConversions--;

	// notify the coordinator
	this->effectLock.unlock();
	this->conversionCv.notify_one();
}

/**
 * Converts the channel's data to RGB pixels.
 */
void EffectRunner::_convertToRgb(DbChannel *channel) {
	HSIPixel *fbPtr = this->fb->data.data();

  // copy the previous frame
  uint8_t *channelBuffer = this->channelBuffers[channel];
	CHECK(channelBuffer != nullptr) << "Don't have output buffer for channel " << channel;

  uint8_t *prevChannelBuffer = this->channelBuffersPrevFrame[channel];

  if(prevChannelBuffer != nullptr) {
    size_t numBytes = channel->numPixels * 3;
    memcpy(prevChannelBuffer, channelBuffer, numBytes);
  }

  // convert pixel data
	for(int i = 0, j = channel->fbOffset; i < channel->numPixels; i++, j++) {
		fbPtr[j].convertToRGB(channelBuffer);
		channelBuffer += 3;
	}
}

/**
 * Converts the channel's data to RGBW pixels.
 */
void EffectRunner::_convertToRgbw(DbChannel *channel) {
	HSIPixel *fbPtr = this->fb->data.data();

  // copy the previous frame
  uint8_t *channelBuffer = this->channelBuffers[channel];
	CHECK(channelBuffer != nullptr) << "Don't have output buffer for channel " << channel;

  uint8_t *prevChannelBuffer = this->channelBuffersPrevFrame[channel];

  if(prevChannelBuffer != nullptr) {
    size_t numBytes = channel->numPixels * 4;
    memcpy(prevChannelBuffer, channelBuffer, numBytes);
  }

	for(int i = 0, j = channel->fbOffset; i < channel->numPixels; i++, j++) {
		fbPtr[j].convertToRGBW(channelBuffer);
		channelBuffer += 4;
	}
}



/**
 * Sends pixel data from each framebuffer to the appropriate nodes.
 */
void EffectRunner::coordinatorSendData(void) {
	// set up the condition variable
	unsigned int outputChannels = this->outputChannels.size();
	this->outstandingSends = outputChannels;

	// handle the case of having zero configured output channels
	if(outputChannels == 0) {
		return;
	}

	// send each channel's data
	for(auto channel : this->outputChannels) {
		// this->workPool->push([this, &channel = channel] (int tid) {
			this->outputPixelData(channel);
		// });
	}

	// wait for the sends to complete
/*	{
		unique_lock<mutex> lk(this->effectLock);
		this->sendingCv.wait(lk, [this]{
			return (this->outstandingSends <= 0);
		});
	}
*/

	// send the multicasted "output enable" command
	this->proto->sendOutputEnableForAllNodes();
}

/**
 * Sends data for one channel.
 */
void EffectRunner::outputPixelData(DbChannel *channel) {
	// validate that the node is ok
	if(channel->node == nullptr) {
		LOG(WARNING) << "Node may not be null!";
		return;
	}

  // get the channel's output buffer
	uint8_t *channelBuffer = this->channelBuffers[channel];
	CHECK(channelBuffer != nullptr) << "Don't have output buffer for channel " << channel;

  // check if the data changed between both frames
  size_t numPixels = channel->numPixels;
  size_t lastChangedPixel = numPixels;

  bool isRGBW = (channel->format == DbChannel::kPixelFormatRGBW);

  uint8_t *prevFrameChannelBuffer = this->channelBuffersPrevFrame[channel];

  if(prevFrameChannelBuffer != nullptr) {
    // reset the counter since we have a buffer
    lastChangedPixel = 0;

    // calculate pixel stride
    size_t pixelStride = (isRGBW == true) ? 4 : 3;

    // check if each pixel matches
    for(size_t pixels = 0; pixels < numPixels; pixels++) {
      uint8_t *prevPixel = &prevFrameChannelBuffer[(pixels * pixelStride)];
      uint8_t *pixel = &channelBuffer[(pixels * pixelStride)];

      // compare the bytes of each pixel
      for(size_t byte = 0; byte < pixelStride; byte++) {
        if(prevPixel[byte] != pixel[byte]) {
          // this byte did not match; store the index
          lastChangedPixel = pixels;
          break;
        }
      }
    }
  }

  // VLOG(1) << lastChangedPixel << " is the last changed pixel out of " << numPixels << " for " << channel;

	// send the data, if any pixels changed
  if(lastChangedPixel > 0) {
    this->proto->sendPixelData(channel, channelBuffer, lastChangedPixel,
                               isRGBW);
  }

	// decrement the outstanding sends and notify coordinator
	this->outstandingSends--;

	this->effectLock.unlock();
	this->sendingCv.notify_one();
}
