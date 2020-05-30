#include "HandlerFactory.h"

#include <iomanip>
#include <sstream>
#include <mutex>

#include <Logging.h>

#include "IController.h"

using namespace Lichtenstein::Server::API;

/// Container for registered classes
HandlerFactory::MapType *HandlerFactory::registrations = nullptr;
/// Class registration lock
std::mutex HandlerFactory::registerLock;

/**
 * Registers a class with the factory.
 *
 * @param tag Arbitrary user-defined tag for the controller
 * @param ctor Constructor function for controller
 * @return Whether the class was registered successfully
 */
bool HandlerFactory::registerClass(const std::string &tag, HandlerCtor ctor) {
    // ensure only one thread can be allocating at a time
    std::lock_guard guard(registerLock);

    // allocate the map if needed
    if(!registrations) {
        registrations = new MapType;
    }

    // store the registration if free
    if(auto it = registrations->find(tag); it == registrations->end()) {
        registrations->insert(std::make_pair(tag, ctor));
        return true;
    }

    // someone already registered this tag
    Logging::error("Illegal re-registration of tag '{}'", tag);
    return false;
}

/**
 * Iterates over all registered controllers.
 */
void HandlerFactory::forEach(std::function<void(const std::string&, HandlerCtor)> f) {
    if(!registrations) return;

    for(auto const &[key, ctor] : *registrations) {
        f(key, ctor);
    }
}

/**
 * Dumps all registered functions
 */
void HandlerFactory::dump() {
    std::stringstream str;

    if(registrations) {
        for(auto const &[key, func] : *registrations) {
            str << std::setw(20) << std::setfill(' ') << key << std::setw(0);
            str << ": " << func << std::endl;
        } 

        Logging::debug("{} REST API handlers registered\n{}", 
            registrations->size(), str.str());
    } else {
        Logging::debug("0 REST API handlers registered");
    }
}
