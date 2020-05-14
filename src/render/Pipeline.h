/**
 * Output pipeline handles which effects run on what groups (or combinations of
 * multiple groups) and then plopping their data out into the framebuffer.
 *
 * The pipeline is responsible for keeping the timing of the effects as well;
 * it will initiate a new frame according to the configured fps value. Output
 * handlers are asynchronously notified as chunks of the framebuffer become
 * available.
 */
#ifndef RENDER_PIPELINE_H
#define RENDER_PIPELINE_H

#include <atomic>
#include <thread>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <future>
#include <chrono>

namespace ctpl {
    class thread_pool;
}

namespace Lichtenstein::Server::Render {
    class IRenderable;
    class IRenderTarget;

    class Pipeline {
        using RenderablePtr = std::shared_ptr<IRenderable>;
        using TargetPtr = std::shared_ptr<IRenderTarget>;
        using RenderPlan = std::unordered_map<TargetPtr, RenderablePtr>;

        using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>; 

        public:
            static void start();
            static void stop();

            // you shouldn't call this
            Pipeline();
            virtual ~Pipeline();

        public:
            double getActualFps() const {
                return this->actualFps;
            }

        private:
            void terminate();

            void workerEntry();
            void readConfig();
      
            std::shared_future<void> submitRenderJob(RenderablePtr, TargetPtr);
            void renderOne(RenderablePtr, TargetPtr);

            void sleep(Timestamp);
            void compensateSleep(long, long);
            void computeActualFps();

        private:
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker = nullptr;

            double targetFps;
            unsigned int numRenderThreads;

            // compensating for nanosleep() inaccuracies
            double sleepInaccuracy;
            double sleepInaccuracySamples;

            // fps accounting
            double actualFps;
            int actualFramesCounter;
            Timestamp fpsStart;

            // render thread pool
            std::unique_ptr<ctpl::thread_pool> pool;

            // render plan modified by function calls. copied at each frame
            RenderPlan plan;
            std::mutex planLock;

        private:
            static std::shared_ptr<Pipeline> sharedInstance;
    };
}

#endif
