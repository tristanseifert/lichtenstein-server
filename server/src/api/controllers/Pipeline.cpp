#include "Pipeline.h"
#include "../Server.h"
#include "../HandlerFactory.h"

#include <string>
#include <functional>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <Format.h>
#include <Logging.h>

#include "db/DataStore.h"
#include "db/DataStorePrimitives+Json.h"

#include "../../render/Pipeline.h"
#include "../../render/HSIPixel.h"
#include "../../render/FillRenderable.h"
#include "../../render/RoutineRenderable.h"
#include "../../render/GroupTarget.h"
#include "../../render/MultiGroupTarget.h"
#include "../../render/BrightnessTransformer.h"

using namespace Lichtenstein::Server::API::Controllers;
using IController = Lichtenstein::Server::API::IController;
using Routine = Lichtenstein::Server::DB::Types::Routine;
using Group = Lichtenstein::Server::DB::Types::Group;
using json = nlohmann::json;

// register to handler factory
bool PipelineController::registered = HandlerFactory::registerClass("Pipeline", 
        PipelineController::construct);

std::unique_ptr<IController> PipelineController::construct(Server *srv) {
    return std::make_unique<PipelineController>(srv);
}


/**
 * Constructs the pipeline controller, registering all of its routes.
 */
PipelineController::PipelineController(Server *srv) : IController(srv) {
    using namespace std::placeholders;

    this->route([this] (auto http) mutable {
        http->Get("/pipeline/", std::bind(&PipelineController::getState, this,_1,_2));
        http->Post("/pipeline/mapping/add", std::bind(&PipelineController::setState, this,_1,_2,_3));
        http->Post("/pipeline/brightness/set", std::bind(&PipelineController::setBrightness, this,_1,_2,_3));
    });
}



/**
 * Gets the current state of the pipeline.
 */
void PipelineController::getState(const ReqType &req, ResType &res) {
    auto pipe = Lichtenstein::Server::Render::Pipeline::pipeline();

    // TODO: add more useful info
    json j;

    // general pipeline statistics
    j["statistics"] = json({
        {"fps", pipe->getActualFps()},
        {"sleepDiff", pipe->getSleepInaccuracy()},
        {"totalFrames", pipe->getTotalFrames()},
    });
    
    // send the response
    this->respond(j, res);
}

/**
 * Creates a mapping in the pipeline between the specified group(s) and the
 * given renderable.
 */
void PipelineController::setState(const ReqType &req, ResType &res,
        const ReaderType &read) {
    using namespace Render;
    
    auto pipe = Lichtenstein::Server::Render::Pipeline::pipeline();
    std::shared_ptr<IRenderable> renderable;
    std::shared_ptr<IRenderTarget> target;

    // parse the request body
    json j;
    this->decode(read, j);
    
    auto renderInfo = j.at("renderable");
    auto targetInfo = j.at("target");

    // create the render target and get its size
    const auto tgtType = targetInfo.at("type").get<std::string>();

    if(tgtType == "groups") {
        target = this->makeGroupTarget(targetInfo);
    } else {
        const auto what = f("Invalid target type '{}'", tgtType);
        throw std::invalid_argument(what);
    }

    if(!target) {
        throw std::runtime_error("Failed to create target");
    }

    const auto numPixels = target->numPixels();

    // create the renderable
    const auto srcType = renderInfo.at("type").get<std::string>();

    if(srcType == "fill") {
        renderable = this->makeRenderableFill(numPixels, renderInfo);
    } else if(srcType == "routine") {
        renderable = this->makeRenderableRoutine(numPixels, renderInfo);
    } else {
        const auto what = f("Invalid renderable type '{}'", srcType);
        throw std::invalid_argument(what);
    }

    if(!renderable) {
        throw std::runtime_error("Failed to create renderable");
    }

    // we're done, create the mapping and reply
    pipe->add(renderable, target);

    json r = {
        {"status", 0}
    };
    this->respond(r, res);
}

/**
 * Sets the brightness of one or more groups.
 */
void PipelineController::setBrightness(const ReqType &req, ResType &res,
        const ReaderType &read) {
    using namespace Render;
    auto pipe = Lichtenstein::Server::Render::Pipeline::pipeline();

    // parse the request body
    json body;
    this->decode(read, body);

    // get the list of group ids and then groups
    auto target = body.at("target");

    std::vector<int> groupIds;
    target.at("groupIds").get_to(groupIds);

    auto groups = DB::DataStore::db()->getSome<Group>(groupIds);

    if(groups.size() != groupIds.size()) {
        Logging::error("Found {} records but got {} ids", groups.size(), 
                groupIds.size());
        throw std::runtime_error("Unable to find all groups");
    }

    // get brightness and create the transformer
    double brightness = 1;
    body.at("brightness").get_to(brightness);

    auto transformer = std::make_shared<BrightnessTransformer>(brightness);

    // add the mapping
    pipe->add(transformer, groups);

    json r = {
        {"status", 0}
    };
    this->respond(r, res);
}



/**
 * Creates a renderable that fills its output with a HSI value.
 *
 * - value: HSIPixel
 */
PipelineController::RenderablePtr 
PipelineController::makeRenderableFill(size_t numPixels, const nlohmann::json &info) {
    using namespace Render;

    auto value = info.at("value").get<HSIPixel>();
    return std::make_shared<FillRenderable>(numPixels, value);
}

/**
 * Creates a renderable that renders a routine from the data store.
 *
 * - routineId: ID of routine in data store
 * - params: ParamMap for routine. Any missing keys are replaced with defaults.
 */
PipelineController::RenderablePtr 
PipelineController::makeRenderableRoutine(size_t numPixels, const nlohmann::json &info) {
    Routine r;
    
    // fetch the routine
    int id = info.at("routineId").get<int>();
    bool found = DB::DataStore::db()->getOne(id, r);

    if(!found) {
        throw std::runtime_error("Failed to find routine");
    }

    // parse params if specified
    decltype(Routine::params) params;
    
    if(info.find("params") != info.end()) {
        params = DB::Types::JsonToParamMap(info.at("params"));
        params.insert(r.params.cbegin(), r.params.cend());
    } else {
        params = r.params;
    }

    // create renderable
    return std::make_shared<Render::RoutineRenderable>(numPixels, r, params);
}



/**
 * Creates a render target for one or more groups.
 */
PipelineController::TargetPtr 
PipelineController::makeGroupTarget(const nlohmann::json &info) {
    using namespace Render;

    // get the list of group ids and then groups
    std::vector<int> groupIds;
    info.at("groupIds").get_to(groupIds);

    auto groups = DB::DataStore::db()->getSome<Group>(groupIds);

    if(groups.size() != groupIds.size()) {
        Logging::error("Found {} records but got {} ids", groups.size(), 
                groupIds.size());
        throw std::runtime_error("Unable to find all groups");
    }

    // create the multi group
    return std::make_shared<MultiGroupTarget>(groups);
}

