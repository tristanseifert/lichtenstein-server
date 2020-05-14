#include "Pipeline.h"
#include "HSIPixel.h"
#include "IRenderable.h"
#include "IRenderTarget.h"
#include "Framebuffer.h"

#include <functional>

#include <ctpl_stl.h>

#include "../Logging.h"
#include "../ConfigManager.h"

#include "FillRenderable.h"
#include "GroupTarget.h"

using thread_pool = ctpl::thread_pool;
using namespace Lichtenstein::Server::Render;

// global rendering pipeline :)
std::shared_ptr<Pipeline> Pipeline::sharedInstance;

/**
 * Initializes the pipeline.
 */
void Pipeline::start() {
    XASSERT(!sharedInstance, "Pipeline is already initialized");
    sharedInstance = std::make_shared<Pipeline>();
}

/**
 * Tears down the pipeline at the earliest opportunity.
 */
void Pipeline::stop() {
    sharedInstance->terminate();
    sharedInstance = nullptr;
}



/**
 * Initializes the rendering pipeline.
 */
Pipeline::Pipeline() {
    // allocate the framebuffer
    this->fb = std::make_shared<Framebuffer>();

    // start up our worker thread
    this->shouldTerminate = false;
    this->worker = std::make_unique<std::thread>(&Pipeline::workerEntry, this);
}
/**
 * Cleans up all resources used by the pipeline.
 */
Pipeline::~Pipeline() {
    /* Ensure the worker has terminated when the destructor is called. This in
     * turn means that all render threads have also terminated. */
    if(!this->shouldTerminate) {
        Logging::error("You should call Render::Pipeline::terminate() before deleting");
        this->terminate(); 
    }
    this->worker->join();
}

/**
 * Requests termination of the renderer.
 */
void Pipeline::terminate() {
    // catch repeated calls
    if(this->shouldTerminate) {
        Logging::error("Ignoring repeated call of Render::Pipeline::terminate()!");
        return;
    }

    // set the termination flag
    Logging::debug("Requesting render pipeline termination");
    this->shouldTerminate = true;
}



/**
 * Entry point for the main rendering thread. This thread is responsible for
 * handling the timing of the renders, distributing work to the work queue, and
 * the overall logistics of getting the final values into the framebuffer so
 * that output plugins can be notified.
 *
 * Note that we let the entire run through the loop finish before we check the
 * termination flag, so worst case terminating the renderer may take 1/fps sec.
 */
void Pipeline::workerEntry() {
    using namespace std::chrono;

    // setup for rendering
    this->readConfig();

    this->pool = std::make_unique<thread_pool>(this->numRenderThreads);

    // initialize some counters
    this->actualFps = -1;
    this->actualFramesCounter = 0;
    this->fpsStart = high_resolution_clock::now();

    this->sleepInaccuracy = this->sleepInaccuracySamples = 0;

    // main loop
    while(!this->shouldTerminate) {
        RenderPlan currentPlan;

        // timestamp the start of frame
        auto start = high_resolution_clock::now();

        // get the render plan for this frame
        this->planLock.lock();
        currentPlan = this->plan;
        this->planLock.unlock();

        if(!currentPlan.empty()) {
            // set up for the rendering
            for(auto const &[target, renderable] : currentPlan) {
                renderable->prepare();
            }

            // dispatch render jobs to the work pool
            std::vector<std::shared_future<void>> jobs;

            for(auto const &[target, renderable] : currentPlan) {
                auto f = this->submitRenderJob(renderable, target);
                jobs.push_back(f);
            }

            // wait for all render jobs to complete and finish
            for(auto const &f : jobs) {
                f.wait();
            }

            for(auto const &[target, renderable] : currentPlan) {
                renderable->finish();
            }
        }

        // sleep to maintain framerate
        this->sleep(start);
    }

    // clean up
    Logging::debug("Render pipeline is shutting down");

    this->pool->stop(false);
    this->pool = nullptr;
}

/**
 * Reads all configuration into our instance variables to cache it.
 */
void Pipeline::readConfig() {
    this->targetFps = ConfigManager::getDouble("render.pipeline.fps", 42);
    this->numRenderThreads = ConfigManager::getUnsigned("render.pipeline.threads", 2);

    Logging::debug("Pipeline fps = {:.1f}; using {} render threads", 
            this->targetFps, this->numRenderThreads);
}



/**
 * Submits a single renderable/target pair to the render queue. Returned is a
 * future that can be used to wait for the job to complete.
 */
std::shared_future<void> Pipeline::submitRenderJob(RenderablePtr render,
        TargetPtr target) {
    using namespace std::placeholders;

    auto fxn = std::bind(&Pipeline::renderOne, this, render, target);
    auto f = this->pool->push(fxn);
    return f.share();
}

/**
 * Executes the renderable's render function, then copies its data into the
 * output framebuffer.
 */
void Pipeline::renderOne(RenderablePtr renderable, TargetPtr target) {
    // start off with the most important part lol
    renderable->render();

    // then, copy out the data into the framebuffer and notify
    target->inscreteFrame(renderable);
}




/**
 * Sleeps enough time to maintain our framerate. This will compensate for drift
 * or other inaccuracies.
 */
void Pipeline::sleep(Timestamp startOfFrame) {
    using namespace std::chrono;

    struct timespec sleep;
    sleep.tv_sec = 0;

    double fpsTimeNs = ((1000 * 1000 * 1000) / this->targetFps);
 
    // sleep for the amount of time required
    auto end = high_resolution_clock::now();
    duration<double, std::nano> difference = (end - startOfFrame);
    long differenceNanos = difference.count();

    sleep.tv_nsec = (fpsTimeNs - differenceNanos);
    sleep.tv_nsec -= this->sleepInaccuracy;
    nanosleep(&sleep, nullptr);

    auto wokenUp = high_resolution_clock::now();

    // calculate fps and irl sleep time
    this->computeActualFps();

    auto sleepTime = high_resolution_clock::now() - end;
    duration<double, std::micro> micros = sleepTime;

    double sleepTimeUs = micros.count();
    double sleepTimeNs = duration<double, std::nano>(sleepTime).count();

    this->compensateSleep(sleep.tv_nsec, sleepTimeNs);
}

/*
 * Naiively calculate a compensation to apply to the nanosleep() call to get us
 * as close as possible to the desired sleep time. We calculate a moving
 * average on the difference between the actual and requested sleep.
 *
 * This algorithm does not handle sudden lag spikes very well.
 */
void Pipeline::compensateSleep(long requested, long actual) {
	double difference = (actual - requested);

	double n = this->sleepInaccuracySamples;
	double oldAvg = this->sleepInaccuracy;

	double newAvg = ((oldAvg * n) + difference) / (n + 1);

	this->sleepInaccuracy = newAvg;
	this->sleepInaccuracySamples++;
}

/**
 * Calculates the actual fps. This counts the number of frames that get
 * processed in a one-second span, and extrapolates fps from this.
 */
void Pipeline::computeActualFps() {
    using namespace std::chrono;

    // increment frames counter and get time difference since measurement start
    this->actualFramesCounter++;

    auto current = high_resolution_clock::now();
    duration<double, std::milli> fpsDifference = (current - this->fpsStart);

    if(fpsDifference.count() >= 1000) {
        // calculate the actual fps and reset counter
        this->actualFps = double(this->actualFramesCounter) /
            (fpsDifference.count() / 1000.f);

        this->actualFramesCounter = 0;
        this->fpsStart = high_resolution_clock::now();
    }
}



/**
 * Adds a mapping of renderable -> target to be dealt with the next time we
 * render a frame.
 *
 * This will ensure that no output group is specified twice. If there is a
 * mapping with the same target, it will be replaced.
 */
void Pipeline::addMapping(RenderablePtr renderable, TargetPtr target) {
    std::lock_guard<std::mutex> lg(this->planLock);

    // TODO: check for duplicate groups

    // insert it
    this->plan[target] = renderable;
}

