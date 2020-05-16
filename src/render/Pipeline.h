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

namespace Lichtenstein::Server::DB::Types {
    class Group;
}

namespace Lichtenstein::Server::Render {
    class Framebuffer;
    class IRenderable;
    class IRenderTarget;
    class IGroupContainer;

    class Pipeline {
        using RenderablePtr = std::shared_ptr<IRenderable>;
        using TargetPtr = std::shared_ptr<IRenderTarget>;
        using GroupContainerPtr = std::shared_ptr<IRenderTarget>;
        using RenderPlan = std::unordered_map<TargetPtr, RenderablePtr>;
        using Group = Lichtenstein::Server::DB::Types::Group;
        using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>; 

        public:
            static void start();
            static void stop();

            static std::shared_ptr<Pipeline> pipeline() {
                return sharedInstance;
            }

            // you shouldn't call this
            Pipeline();
            virtual ~Pipeline();

        public:
            void add(RenderablePtr renderable, TargetPtr target);
            TargetPtr add(RenderablePtr renderable, const Group &g);
            TargetPtr add(RenderablePtr renderable, const std::vector<Group> &g);

            void remove(TargetPtr target);

            void dump();

            /**
             * Gets the actual framerate accomplished by the renderer.
             */
            double getActualFps() const {
                return this->actualFps;
            }
            /**
             * Gets the calculated inaccuracy of the sleep. The return value is
             * in nanoseconds.
             */
            double getSleepInaccuracy() const {
                return this->sleepInaccuracy;
            }
            /**
             * Gets the total number of frames this pipeline has rendered.
             */
            unsigned long long getTotalFrames() const {
                return this->totalFrames;
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
            std::shared_ptr<Framebuffer> fb;

            // worker thread
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker;

            // configuration
            double targetFps;
            unsigned int numRenderThreads;

            // compensating for nanosleep() inaccuracies
            double sleepInaccuracy = 0;
            double sleepInaccuracySamples = 0;

            // fps accounting
            double actualFps = -1;
            int actualFramesCounter;
            Timestamp fpsStart;

            unsigned long long totalFrames = 0;

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
