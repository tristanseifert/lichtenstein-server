#include "Groups.h"

#include <sstream>
#include <vector>
#include <functional>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../../version.h"
#include "../../Logging.h"

#include "../../db/DataStore.h"
#include "../../db/DataStorePrimitives+Json.h"

#include "../HandlerFactory.h"

using IController = Lichtenstein::Server::API::IController;
using namespace Lichtenstein::Server::API::Controllers;
using Group = Lichtenstein::Server::DB::Types::Group;
using json = nlohmann::json;

// register to handler factory
bool Groups::registered = HandlerFactory::registerClass("Groups", 
        Groups::construct);

std::unique_ptr<IController> Groups::construct(Server *srv) {
    return std::make_unique<Groups>(srv);
}


/**
 * Constructs the Groups controller, registering all of its routes.
 */
Groups::Groups(Server *srv) : IController(srv) {
    using namespace std::placeholders;

    this->route([this] (auto http) mutable {
        http->Get("/groups/", std::bind(&Groups::getAll, this,_1,_2));
        
        http->Get(R"(/groups/(\d+))", std::bind(&Groups::getOne, this,_1,_2));
        http->Delete(R"(/groups/(\d+))", std::bind(&Groups::remove, this,_1,_2));
        http->Put(R"(/groups/(\d+))", std::bind(&Groups::update, this,_1,_2,_3));
        
        http->Post("/groups/new", std::bind(&Groups::create, this,_1,_2,_3));
    });
}



/**
 * Returns a list of all groups in the data store. 
 */
void Groups::getAll(const ReqType &req, ResType &res) {
    auto groups = DB::DataStore::db()->getAll<Group>();

    json j = {
        {"status", 0},
        {"count", groups.size()},
        {"records", groups}
    };
    this->respond(j, res);
}
/**
 * Returns information on a single group in the data store.
 */
void Groups::getOne(const ReqType &req, ResType &res) {
    Group g;

    // try to load the group from db
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Group>(id, g)) {
        json j = {
            {"status", 0},
            {"record", g}
        };
        this->respond(j, res);
    } 
    // nothing found by this ID
    else {
        res.status = 404;
    }
}

/**
 * Creates a new group.
 */
void Groups::create(const ReqType &req, ResType &res, const ReaderType &read) {
    // read the JSON body and create a group from it
    json toInsert;
    this->decode(read, toInsert);
    
    auto group = toInsert.get<Group>();
    int id = DB::DataStore::db()->insert(group);

    if(id) {
        // update the group object and output it
        group.id = id;

        json j = {
            {"status", 0},
            {"record", group}
        };
        this->respond(j, res);
    } else {
        res.status = 500;
    }
}

/**
 * Updates an existing group.
 */
void Groups::update(const ReqType &req, ResType &res, const ReaderType &read) {
    Group g;

    // attempt to fetch the group from data store
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Group>(id, g)) {
        // parse the request body
        json toInsert;
        this->decode(read, toInsert);
    
        auto updateFields = toInsert.get<Group>();
        
        // merge the changeable properties
        g.name = updateFields.name;
        g.enabled = updateFields.enabled;
        g.startOff = updateFields.startOff;
        g.endOff = updateFields.endOff;

        // update the timestamp and write to db
        g.updateLastModified();
        DB::DataStore::db()->update(g);

        // send result
        json j = {
            {"status", 0},
            {"record", g}
        };
        this->respond(j, res);
    } 
    // no group with that ID
    else {
        res.status = 404;
    }
}

/**
 * Deletes an existing group.
 */
void Groups::remove(const ReqType &req, ResType &res) {
    Group g;

    // fetch the group from the data store so we can delete it
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Group>(id, g)) {
        DB::DataStore::db()->remove(g);

        // send result
        json j = {
            {"status", 0},
        };
        this->respond(j, res);
    } 
    // no group with that ID
    else {
        res.status = 404;
    }
}
