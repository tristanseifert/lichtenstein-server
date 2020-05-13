#include "Pipeline.h"
#include "HSIPixel.h"

#include "../Logging.h"
#include "../ConfigManager.h"

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
    // setup stage
    this->readConfig();

    // main loop
    while(!this->shouldTerminate) {

    }

    // clean up
    Logging::debug("Render pipeline is shutting down");
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
