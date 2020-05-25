#include "Routines.h"

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
using Routine = Lichtenstein::Server::DB::Types::Routine;
using json = nlohmann::json;

// register to handler factory
bool Routines::registered = HandlerFactory::registerClass("Routines", 
        Routines::construct);

std::unique_ptr<IController> Routines::construct(Server *srv) {
    return std::make_unique<Routines>(srv);
}


/**
 * Constructs the Routines controller, registering all of its routes.
 */
Routines::Routines(Server *srv) : IController(srv) {
    using namespace std::placeholders;

    this->route([this] (auto http) mutable {
        http->Get("/routines/", std::bind(&Routines::getAll, this,_1,_2));
        
        http->Get(R"(/routines/(\d+))", std::bind(&Routines::getOne, this,_1,_2));
        http->Delete(R"(/routines/(\d+))", std::bind(&Routines::remove, this,_1,_2));
        http->Put(R"(/routines/(\d+))", std::bind(&Routines::update, this,_1,_2,_3));
        
        http->Post("/routines/new", std::bind(&Routines::create, this,_1,_2,_3));
    });
}



/**
 * Returns a list of all routines in the data store. 
 */
void Routines::getAll(const ReqType &req, ResType &res) {
    auto routines = DB::DataStore::db()->getAll<Routine>();

    json j = {
        {"status", 0},
        {"count", routines.size()},
        {"records", routines}
    };
    this->respond(j, res);
}
/**
 * Returns information on a single routine in the data store.
 */
void Routines::getOne(const ReqType &req, ResType &res) {
    Routine r;

    // try to load the routine from db
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Routine>(id, r)) {
        json j = {
            {"status", 0},
            {"record", r}
        };
        this->respond(j, res);
    } 
    // nothing found by this ID
    else {
        res.status = 404;
    }
}

/**
 * Creates a new routine.
 */
void Routines::create(const ReqType &req, ResType &res, const ReaderType &read) {
    // read the JSON body and create a routine from it
    json toInsert;
    this->decode(read, toInsert);
    
    auto routine = toInsert.get<Routine>();
    int id = DB::DataStore::db()->insert(routine);

    if(id) {
        // update the routine object and output it
        routine.id = id;

        json j = {
            {"status", 0},
            {"record", routine}
        };
        this->respond(j, res);
    } else {
        res.status = 500;
    }
}

/**
 * Updates an existing routine.
 */
void Routines::update(const ReqType &req, ResType &res, const ReaderType &read) {
    Routine r;

    // attempt to fetch the routine from data store
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Routine>(id, r)) {
        // parse the request body
        json toInsert;
        this->decode(read, toInsert);
    
        auto updateFields = toInsert.get<Routine>();
        
        // merge the changeable properties
        r.name = updateFields.name;
        r.code = updateFields.code;
        r.params = updateFields.params;

        // update the timestamp and write to db
        r.updateLastModified();
        DB::DataStore::db()->update(r);

        // send result
        json j = {
            {"status", 0},
            {"record", r}
        };
        this->respond(j, res);
    } 
    // no routine with that ID
    else {
        res.status = 404;
    }
}

/**
 * Deletes an existing routine.
 */
void Routines::remove(const ReqType &req, ResType &res) {
    Routine r;

    // fetch the routine from the data store so we can delete it
    int id = std::stoi(std::string(req.matches[1]), nullptr, 10);
    if(DB::DataStore::db()->getOne<Routine>(id, r)) {
        DB::DataStore::db()->remove(r);

        // send result
        json j = {
            {"status", 0},
        };
        this->respond(j, res);
    } 
    // no routine with that ID
    else {
        res.status = 404;
    }
}
