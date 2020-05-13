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

namespace Lichtenstein::Server::Render {
    class Pipeline {
        public:
            static void start();
            static void stop();

            // you shouldn't call this
            Pipeline();
            virtual ~Pipeline();

        public:

        private:
            void terminate();

            void workerEntry();
            void readConfig();
        
        private:
            std::atomic_bool shouldTerminate;
            std::unique_ptr<std::thread> worker = nullptr;

            double targetFps;
            unsigned int numRenderThreads;

        private:
            static std::shared_ptr<Pipeline> sharedInstance;
    };
}

#endif
