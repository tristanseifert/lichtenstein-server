#include "NodeChannels.h"

#include <sstream>
#include <vector>
#include <functional>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "Logging.h"

#include "db/DataStore.h"
#include "db/DataStorePrimitives+Json.h"

#include "../HandlerFactory.h"

using IController = Lichtenstein::Server::API::IController;
using namespace Lichtenstein::Server::API::Controllers;
using NodeChannel = Lichtenstein::Server::DB::Types::NodeChannel;
using json = nlohmann::json;

// register to handler factory
bool NodeChannels::registered = HandlerFactory::registerClass("NodeChannels", 
        NodeChannels::construct);

std::unique_ptr<IController> NodeChannels::construct(Server *srv) {
    return std::make_unique<NodeChannels>(srv);
}


/**
 * Constructs the NodeChannels controller, registering all of its routes.
 */
NodeChannels::NodeChannels(Server *srv) : IController(srv) {
    using namespace std::placeholders;

    this->route([this] (auto http) mutable {
        http->Get("/channels/", std::bind(&NodeChannels::getAll, this,_1,_2));
        //http->Get(R"(/nodes/(\d+)/chanels)", std::bind(&NodeChannels::getAllForNode, this,_1,_2));
        http->Get(R"(/channels/(\d+))", std::bind(&NodeChannels::getOne, this,_1,_2));
        http->Put(R"(/channels/(\d+))", std::bind(&NodeChannels::update, this,_1,_2,_3));
    });
}



/**
 * Returns a list of all channels in the data store. 
 */
void NodeChannels::getAll(const ReqType &req, ResType &res) {
    auto channels = DB::DataStore::db()->getAll<NodeChannel>();

    json j = {
        {"status", 0},
        {"count", channels.size()},
        {"records", channels}
    };
    this->respond(j, res);
}
/**
 * Returns information on a single channel in the data store.
 */
void NodeChannels::getOne(const ReqType &req, ResType &res) {
    NodeChannel c;

    // try to load the channel from db
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<NodeChannel>(id, c)) {
        json j = {
            {"status", 0},
            {"record", c}
        };
        this->respond(j, res);
    } 
    // nothing found by this ID
    else {
        res.status = 404;
    }
}

/**
 * Updates an existing channel.
 */
void NodeChannels::update(const ReqType &req, ResType &res, const ReaderType &read) {
    NodeChannel c;

    // attempt to fetch the channel from data store
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<NodeChannel>(id, c)) {
        // parse the request body
        json toInsert;
        this->decode(read, toInsert);
    
        auto updateFields = toInsert.get<NodeChannel>();
        
        // merge the changeable properties
        c.label = updateFields.label;

        // update the timestamp and write to db
        c.updateLastModified();
        DB::DataStore::db()->update(c);

        // send result
        json j = {
            {"status", 0},
            {"record", c}
        };
        this->respond(j, res);
    } 
    // no group with that ID
    else {
        res.status = 404;
    }
}

