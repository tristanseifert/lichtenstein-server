#ifndef RPI_PLUGIN_H
#define RPI_PLUGIN_H

#include <memory>
#include <vector>
#include <mutex>

#include "ws2811.h"

namespace Lichtenstein::Client::Output {
    class PluginManager;
    class IOutputChannel;
}

namespace Lichtenstein::Plugin::RPi {
    class OutputChannel;

    class Plugin {
        friend class OutputChannel;

        public:
            Plugin(Client::Output::PluginManager *);
            ~Plugin();

            int start(std::vector<std::shared_ptr<Client::Output::IOutputChannel>> &);
            int stop();

        private:
            void willOutputChannel(size_t channel);

        private:
            // plugin manager (usable for reading config)
            Client::Output::PluginManager *manager = nullptr;

            // initialized output channels
            std::vector<std::shared_ptr<OutputChannel>> channels;
            // number of channels that have output
            size_t numOutput = 0;

            // lock around rendering
            std::mutex driverLock;
            // ws2811 driver config
            ws2811_t driver;
    };

};

#endif
