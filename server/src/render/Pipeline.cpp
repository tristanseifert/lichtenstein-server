#include "Pipeline.h"
#include "Framebuffer.h"
#include "HSIPixel.h"
#include "IRenderable.h"
#include "IRenderTarget.h"
#include "IGroupContainer.h"
#include "IPixelTransformer.h"

#include <functional>
#include <iomanip>
#include <sstream>
#include <tuple>

#include <ctpl_stl.h>

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include "FillRenderable.h"
#include "GroupTarget.h"
#include "MultiGroupTarget.h"

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
        TransformPlan currentTrans;

        // timestamp the start of frame
        auto start = high_resolution_clock::now();

        // get the render plan and transforms for this frame
        this->planLock.lock();
        currentPlan = this->plan;
        this->planLock.unlock();

        this->transformsLock.lock();
        currentTrans = this->transforms;
        this->transformsLock.unlock();

        if(!currentPlan.empty()) {
            auto token = this->fb->startFrame();
            
            // set up for the rendering
            for(auto const &[target, renderable] : currentPlan) {
                renderable->lock();
                renderable->prepare();
                renderable->unlock();
            }

            // dispatch render jobs to the work pool
            using JobTuple = std::tuple<std::shared_future<void>, 
                  TargetPtr, RenderablePtr>;
            std::vector<JobTuple> jobs;

            for(auto const &[target, renderable] : currentPlan) {
                auto future = this->submitRenderJob(renderable, target);
                jobs.push_back(std::make_tuple(future, target, renderable));
            }

            // wait for all render jobs to complete and finish
            for(auto const &job : jobs) {
                auto future = std::get<0>(job);
                auto target = std::get<1>(job);
                auto renderable = std::get<2>(job);

                // this will re-throw the exception if we get one
                try {
                    future.wait();
                    future.get();
                } catch(std::exception &ex) {
                    // the renderable is in an unknown state. get rid of it
                    Logging::error("Exception while evaluating {}: {}", 
                            (void*)renderable.get(), ex.what());
                    currentPlan.erase(target);

                    // try to remove it from the main plan as well
                    this->remove(target);
                }
            }
            jobs.clear();

            for(auto const &[target, renderable] : currentPlan) {
                renderable->lock();
                renderable->finish();
                renderable->unlock();
            }

            // apply transformations
            for(auto it = currentTrans.begin(); it != currentTrans.end();) {
                auto range = it->first;
                auto transformer = it->second;
                try {
                    transformer->lock();
                    transformer->transform(this->fb, range);
                    transformer->unlock();

                    ++it;
                } catch(std::exception &ex) {
                    // the transform is in an unknown state. get rid of it
                    Logging::error("Exception in transformer {}: {}", 
                            (void*)transformer.get(), ex.what());
                    // attempt to remove it
                    it = currentTrans.erase(it);
                    this->remove(transformer);
                }
            }

            // end frame rendering; this will send data and sync
            this->fb->endFrame(token);
        }

        // sleep to maintain framerate
        this->totalFrames++;
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
    XASSERT(render, "Renderable is required");
    XASSERT(target, "Target is required");

    auto fxn = std::bind(&Pipeline::renderOne, this, render, target);
    auto f = this->pool->push(fxn);
    return f.share();
}

/**
 * Executes the renderable's render function, then copies its data into the
 * output framebuffer.
 */
void Pipeline::renderOne(RenderablePtr renderable, TargetPtr target) {
    // acquire locks
    renderable->lock();

    // render and copy data out
    renderable->render();
    target->inscreteFrame(this->fb, renderable);

    // release locks
    renderable->unlock();
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
 * mapping with the same target, it will be replaced, or an exception thrown
 * depending on the `remove` argument.
 */
void Pipeline::add(RenderablePtr renderable, TargetPtr target, bool remove) {
    using std::dynamic_pointer_cast;

    if(!renderable) {
        throw std::invalid_argument("Renderable is required");
    } else if(!target) {
        throw std::invalid_argument("Target is required");
    }

    std::lock_guard<std::mutex> lg(this->planLock);

    auto inContainer = dynamic_pointer_cast<IGroupContainer>(target);

    // is the input mapping a container?
    if(inContainer) {
        // iterate over all targets and see if they intersect with this one
        for(auto it = this->plan.cbegin(); it != this->plan.cend(); ) {
            auto t = it->first;
            auto const r = it->second;

            // is this entry a group container?
            auto c = dynamic_pointer_cast<IGroupContainer>(t);
            if(!c) {
                goto next;
            }

            // check whether there are any conflicts
            if(c->contains(*inContainer)) {
                Logging::debug("Conflict between input {} and entry {}",
                        *inContainer, *c);

                // are the groups identical?
                if(*c == *inContainer) {
                    // if so, replace this entry
                    Logging::trace("Identical groups in existing container; removing existing");

                    if(remove) {
                        it = this->plan.erase(it);
                        goto beach;
                    } else {
                        auto what = f("Conflict with group {}", *c);
                        throw std::runtime_error(what);
                    }
                }
                // they aren't, but the container is mutable
                else if(c->isMutable()) {
                    // abort if we cannot remove anything
                    if(!remove) {
                        auto what = f("Conflict with mutable container {}", *c);
                        throw std::runtime_error(what);
                    }

                    // update the target by removing the intersection
                    std::vector<int> intersection;
                    c->getUnion(*inContainer, intersection);

                    Logging::trace("Removing {} groups from conflicting entry",
                            intersection.size());

                    t->lock();
                    for(const int id : intersection) {
                        c->removeGroup(id);
                    }
                    t->unlock();

                    // if this emptied the conflicting group, remove it
                    if(t->numPixels() == 0) {
                        Logging::trace("Removing empty conflicting target and inserting");
                        it = this->plan.erase(it);
                        continue;
                    }

                    // take a lock on the renderable and resize
                    r->lock();

                    auto requiredSize = t->numPixels();
                    Logging::trace("Resizing renderable {} to {} pixels",
                            (void*) r.get(), requiredSize);
                    r->resize(requiredSize);

                    r->unlock();
                }
                // if the conflicting container is immutable but only one remove it
                else if(c->numGroups() == 1) {
                    Logging::trace("Removing single group conflicting entry");

                    if(remove) {
                        it = this->plan.erase(it);
                        continue;
                    } else {
                        auto what = f("Conflict with single entry group {}", *c);
                        throw std::runtime_error(what);
                    }
                }
                // container is immutable so we can't handle this
                else {
                    Logging::trace("Immutable container, cannot satisfy mapping");
                    throw std::runtime_error("Unable to add mapping");
                }
            }

            // there are no conflicts with this container, check the next
next: ;
            ++it;
        }

beach: ;
        // if we get here, go ahead and insert it. conflicts have been resolved
        this->plan[target] = renderable;
    }
    // it is not; we should just insert it
    else {
        Logging::warn("Inserting non-container render target {}",
                (void*)target.get());

        this->plan[target] = renderable;
    }
}
/**
 * Removes the mapping to the given target.
 */
void Pipeline::remove(TargetPtr target) {
    if(!target) {
        throw std::invalid_argument("Target is required");
    }

    std::lock_guard<std::mutex> lg(this->planLock);

    if(this->plan.find(target) == this->plan.end()) {
        throw std::runtime_error("No such target in render pipeline");
    }
    
    this->plan.erase(target);
}

/**
 * Adds a single group with the given renderable to the pipeline.
 */
Pipeline::TargetPtr Pipeline::add(RenderablePtr renderable, const Group &g, bool remove) {
    auto t = std::make_shared<GroupTarget>(g);
    this->add(renderable, t, remove);
    return t;
}

/**
 * Creates a multigroup from the specified list of groups ands adds it to the
 * render pipeline.
 */
Pipeline::TargetPtr Pipeline::add(RenderablePtr renderable, 
        const std::vector<Group> &g, bool remove) {
    auto t = std::make_shared<MultiGroupTarget>(g);
    this->add(renderable, t, remove);
    return t;
}



/**
 * Adds a new transformer to the pipeline that operates over the given range.
 * If there are any conflicts with existing transformers, they will either be
 * removed or an error returned, depending on the `remove` argument.
 */
void Pipeline::add(TransformerPtr transform, const FbRange &range, bool remove) {
    if(!transform) {
        throw std::invalid_argument("Transformer is required");
    }

    std::lock_guard<std::mutex> lg(this->transformsLock);

    // check to see if there's any overlapping ranges
    for(auto it = this->transforms.begin(); it != this->transforms.end();) {
        if(it->first.intersects(range)) {
            if(remove) {
                Logging::trace("Removing conflicting range {}", it->first);
                it = this->transforms.erase(it);
                continue;
            } else {
                auto what = f("Conflict with range {}", it->first);
                throw std::runtime_error(what);
            }
        }
    }

    // all conflicts were removed or there were none. insert it
    this->transforms[range] = transform;
}
/**
 * Creates a single entry for a range encompassing the given group.
 */
void Pipeline::add(TransformerPtr transformer, const Group &g, bool remove) {
    this->add(transformer, FbRange(g.startOff, (g.endOff - g.startOff)), remove);
}

/**
 * Creates an entry for a range covering each of the specified groups.
 */
void Pipeline::add(TransformerPtr transformer,const std::vector<Group> &groups,
        bool remove) {
    for(auto const &group : groups) {
        this->add(transformer, group, remove);
    }
}

/**
 * Removes a transformer from the transforms map.
 */
void Pipeline::remove(TransformerPtr transform) {
    if(!transform) {
        throw std::invalid_argument("Transformer is required");
    }

    std::lock_guard<std::mutex> lg(this->transformsLock);

    // since we're searching by value, traverse the entire map
    for(auto it = this->transforms.begin(); it != this->transforms.end();) {
        if(it->second == transform) {
            it = this->transforms.erase(it);
            return;
        }
    }

    // if we get down here, we couldn't find it in the list
    throw std::runtime_error("No such transform");
}
/**
 * Removes transformers for the given range. If it is an exact match, only that
 * entry is removed; otherwise, all ranges that overlap with this range are
 * removed.
 */
void Pipeline::remove(const FbRange &range) {
    std::lock_guard<std::mutex> lg(this->transformsLock);
    
    // can we find the exact range?
    if(this->transforms.find(range) != this->transforms.end()) {
        this->transforms.erase(range);
        return;
    }

    // iterate all ranges and remove intersecting ones
    for(auto it = this->transforms.begin(); it != this->transforms.end();) {
        if(it->first.intersects(range)) {
            it = this->transforms.erase(it);
        } else {
            ++it;
        }
    }
}




/**
 * Dumps the current output mapping to the log output.
 */
void Pipeline::dump() {
    std::lock_guard<std::mutex> lg(this->planLock);
    std::stringstream out;
        
    // loop over the entire plan
    for(auto it = this->plan.begin(); it != this->plan.end(); it++) {
        if(it != this->plan.begin()) out << std::endl;
            
        auto target = it->first;
        auto renderable = it->second;

        auto container = std::dynamic_pointer_cast<IGroupContainer>(target);

        // print container value
        out << std::setw(20) << std::setfill(' ');
        if(container) {
            out << *container;
        } else {
            out << std::hex << (uintptr_t) target.get();
        }
        out << std::setw(0);

        // print the renderable
        out << " 0x" << std::hex << (uintptr_t) renderable.get();
    }

    // print that shit
    Logging::debug("Pipeline state\n{}", out.str());
}

