#include "DataStorePrimitives+Json.h"

#include <sstream>
#include <stdexcept>
#include <variant>

#include <nlohmann/json.hpp>

#include "DataStorePrimitives.h"

using json = nlohmann::json;


namespace Lichtenstein::Server::DB::Types {
/**
 * Serializes a ParamMap to json.
 */
static json ParamMapToJson(const ParamMapType &m) {
    json j;

    for(auto const &[key, value] : m) {
        if(std::holds_alternative<bool>(value)) {
            j[key] = std::get<bool>(value);
        }
        else if(std::holds_alternative<double>(value)) {
            j[key] = std::get<double>(value);
        }
        else if(std::holds_alternative<uint64_t>(value)) {
            j[key] = std::get<uint64_t>(value);
        }
        else if(std::holds_alternative<int64_t>(value)) {
            j[key] = std::get<int64_t>(value);
        }
        else if(std::holds_alternative<std::string>(value)) {
            j[key] = std::get<std::string>(value);
        } else {
            std::stringstream what;
            what << "Unable to serialize type id " << value.index() 
                << " to json";
            throw std::runtime_error(what.str());
        } 
    }

    return j;
}
/**
 * Converts JSON to a ParamMap.
 */
static ParamMapType JsonToParamMap(const json &j) {
    ParamMapType m;

    // iterate over each key in the json object
    for(auto it = j.begin(); it != j.end(); ++it) {
        const auto key = it.key();
        auto value = it.value();

        // insert the proper representation of it
        if(value.is_boolean()) {
            m[key] = value.get<bool>();
        } else if(value.is_number_float()) {
            m[key] = value.get<double>();
        } else if(value.is_number_unsigned()) {
            m[key] = value.get<uint64_t>();
        } else if(value.is_number_integer()) {
            m[key] = value.get<int64_t>();
        } else if(value.is_string()) {
            m[key] = value.get<std::string>();
        } else {
            std::stringstream what;
            what << "Unable to convert json value '" << value << "'";

            throw std::runtime_error(what.str());
        }
    }

    // only if we get down here did everything convert as planned
    return m;
}



/**
 * JSON (de)serialization for routines
 */
void to_json(json &j, const Routine &r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"code", r.code},
        {"params", ParamMapToJson(r.params)},
        {"lastModified", r.lastModified}
    };
}

void from_json(const json &j, Routine &r) {
    // ID is optional when reading from json
    try {
        r.id = j.at("id").get<int>();
    } catch(json::out_of_range &) {
        r.id = -1;
    }

    r.name = j.at("name").get<std::string>();
    r.code = j.at("code").get<std::string>();

    // params can be omitted
    try {
        r.params = JsonToParamMap(j.at("params"));
    } catch(json::out_of_range &) {}
}



/**
 * JSON (de)serialization for groups
 */
void to_json(json &j, const Group &g) {
    j = {
        {"id", g.id},
        {"name", g.name},
        {"enabled", g.enabled},
        {"start", g.startOff},
        {"end", g.endOff},
        {"brightness", g.brightness},

        {"routineId", nullptr},
        {"routineState", nullptr},
        
        {"lastModified", g.lastModified}
    };

    // is there a routine state?
    if(g.routineId) {
        j["routineId"] = *g.routineId;
        j["routineState"] = ParamMapToJson(*g.routineState);
    }
}

void from_json(const json &j, Group &g) {
    // ID is optional when reading from json
    try {
        g.id = j.at("id").get<int>();
    } catch(json::out_of_range &) {
        g.id = -1;
    }

    // mandatory fields
    g.name = j.at("name").get<std::string>();
    g.enabled = j.at("enabled").get<bool>();
    g.startOff = j.at("start").get<int>();
    g.endOff = j.at("end").get<int>();

    // routine id or state is _not_ input from json
} 



/**
 * JSON (de)serialization for nodes
 */
void to_json(json &j, const Node &n) {
    j = {
        {"id", n.id},
        {"label", nullptr},
        {"address", n.address},
        {"hostname", n.hostname},
        {"versions", json({
            {"sw", n.swVersion},
            {"hw", n.hwVersion},
        })},
        {"lastCheckin", n.lastCheckin},
        {"lastModified", n.lastModified}
    };

    if(n.label) {
        j["label"] = *n.label;
    }
}

void from_json(const json &j, Node &n) {
    // ID is optional when reading from json
    try {
        n.id = j.at("id").get<int>();
    } catch(json::out_of_range &) {
        n.id = -1;
    }

    // if label is omitted, set it to null
    try {
        n.label = std::make_shared<std::string>(j.at("label").get<std::string>());
    } catch(json::out_of_range &) {
        n.label = nullptr;
    }
}



/**
 * JSON (de)serialization for node channels
 */
void to_json(json &j, const NodeChannel &c) {
    j = {
        {"id", c.id},
        {"nodeId", c.nodeId},
        {"label", nullptr},
        {"index", c.nodeChannelIndex},
        {"numPixels", c.numPixels},
        {"fbOffset", c.fbOffset},
        {"format", c.format},
        {"lastModified", c.lastModified}
    };

    if(c.label) {
        j["label"] = *c.label;
    }
}

void from_json(const json &j, NodeChannel &c) {
    // ID is optional when reading from json
    try {
        c.id = j.at("id").get<int>();
    } catch(json::out_of_range &) {
        c.id = -1;
    }

    // if label is omitted, set it to null
    try {
        c.label = std::make_shared<std::string>(j.at("label").get<std::string>());
    } catch(json::out_of_range &) {
        c.label = nullptr;
    }

    // framebuffer offset is mandatory
    c.fbOffset = j.at("fbOffset").get<int>();
}
}
