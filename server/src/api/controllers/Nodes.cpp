#include "Nodes.h"

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
using Node = Lichtenstein::Server::DB::Types::Node;
using json = nlohmann::json;

// register to handler factory
bool Nodes::registered = HandlerFactory::registerClass("Nodes", 
        Nodes::construct);

std::unique_ptr<IController> Nodes::construct(Server *srv) {
    return std::make_unique<Nodes>(srv);
}


/**
 * Constructs the Nodes controller, registering all of its routes.
 */
Nodes::Nodes(Server *srv) : IController(srv) {
    using namespace std::placeholders;

    this->route([this] (auto http) mutable {
        http->Get("/nodes/", std::bind(&Nodes::getAll, this,_1,_2));
        http->Get(R"(/nodes/(\d+))", std::bind(&Nodes::getOne, this,_1,_2));
        http->Put(R"(/nodes/(\d+))", std::bind(&Nodes::update, this,_1,_2,_3));
    });
}



/**
 * Returns a list of all nodes in the data store. 
 */
void Nodes::getAll(const ReqType &req, ResType &res) {
    auto nodes = DB::DataStore::db()->getAll<Node>();

    json j = {
        {"status", 0},
        {"count", nodes.size()},
        {"records", nodes}
    };
    this->respond(j, res);
}
/**
 * Returns information on a single node in the data store.
 */
void Nodes::getOne(const ReqType &req, ResType &res) {
    Node n;

    // try to load the node from db
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Node>(id, n)) {
        json j = {
            {"status", 0},
            {"record", n}
        };
        this->respond(j, res);
    } 
    // nothing found by this ID
    else {
        res.status = 404;
    }
}

/**
 * Updates an existing node.
 */
void Nodes::update(const ReqType &req, ResType &res, const ReaderType &read) {
    Node n;

    // attempt to fetch the node from data store
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Node>(id, n)) {
        // parse the request body
        json toInsert;
        this->decode(read, toInsert);
    
        auto updateFields = toInsert.get<Node>();
        
        // merge the changeable properties
        n.label = updateFields.label;

        // update the timestamp and write to db
        n.updateLastModified();
        DB::DataStore::db()->update(n);

        // send result
        json j = {
            {"status", 0},
            {"record", n}
        };
        this->respond(j, res);
    } 
    // no group with that ID
    else {
        res.status = 404;
    }
}

