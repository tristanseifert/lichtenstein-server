#include "DataStorePrimitives.h"

#include <sstream>
#include <stdexcept>
#include <chrono>
#include <stdexcept>

#include <Format.h>
#include <Logging.h>

#include <uuid.h>

// serializing params
#include <cista.h>

#include "SerializedStructs.h"

using namespace Lichtenstein::Server::DB::Types;
using namespace Lichtenstein::Server::DB::Types::IceChest;

/**
 * Deserializes a byte array back to an unordered map.
 */
static void deserialize(const std::vector<char> &bytes, ParamMapType &map) {
    // create a message reader from the blob
    auto msg = cista::deserialize<RoutineParams, kCistaMode>(bytes);

    for(const auto& entry : msg->params) {
        // get the value into the variant type in the map
        const std::string key = entry.first.str();
        const auto &value = entry.second;

        switch(value.type) {
            // Boolean
            case TYPE_BOOL:
                map.emplace(key, value.v.boolean);
                break;
            // Floating point 
            case TYPE_FLOAT:
                map.emplace(key, value.v.numFloat);
                break;
            // Unsigned integer
            case TYPE_UNSIGNED:
                map.emplace(key, value.v.numUnsigned);
                break;
            // Signed integer
            case TYPE_SIGNED:
                map.emplace(key, value.v.numSigned);
                break;
            // Character string
            case TYPE_STRING:
                map.emplace(key, value.v.str.str());
                break;

            // unknown type
            default: {
                std::stringstream what;
                what << "Invalid type '" << (int) value.type << "' for key '" << key << "'";

                throw std::invalid_argument(what.str());
                break;
            }
        }
    }
}
/**
 * Serializes an unordered map into a byte array.
 */
static void serialize(const ParamMapType &map, std::vector<char> &bytes) {
    // build the struct
    RoutineParams data;

    // iterate over each entry in the map
    int i = 0;
    for (const auto& [key, value] : map) {
        RoutineParamTypeContainer container;
        container.type = TYPE_NULL;

        // copy over the key and value
        if(std::holds_alternative<bool>(value)) {
            container.type = TYPE_BOOL;
            container.v.boolean = std::get<bool>(value);
        }
        else if(std::holds_alternative<double>(value)) {
            container.type = TYPE_FLOAT;
            container.v.numFloat = std::get<double>(value);
        }
        else if(std::holds_alternative<uint64_t>(value)) {
            container.type = TYPE_UNSIGNED;
            container.v.numUnsigned = (std::get<uint64_t>(value));
        }
        else if(std::holds_alternative<int64_t>(value)) {
            container.type = TYPE_SIGNED;
            container.v.numSigned = (std::get<int64_t>(value));
        }
        else if(std::holds_alternative<std::string>(value)) {
            container.type = TYPE_STRING;
            container.v.str = (std::get<std::string>(value));
        }
        else {
            std::stringstream what;
            what << "Invalid type '" << value.index() << "' for key '" 
                << key << "''"; 

            throw std::invalid_argument(what.str());
        }

        // stick it in
        data.params[key] = container;

        // go to the next entry
        i++;
    }

    // update the blob
    const auto infoData = cista::serialize<kCistaMode>(data);
    bytes.assign(infoData.begin(), infoData.end());

    Logging::debug("Serialized params into {} bytes", infoData.size());
}



/**
 * Updates the last modified timestamp to the current timestamp.
 */
void BaseType::updateLastModified() {
    using namespace std::chrono;

    const auto p1 = system_clock::now();
    const auto sec = duration_cast<seconds>(p1.time_since_epoch()).count();

    this->lastModified = sec;
}




/**
 * Deserializes the routine default params
 */
void Routine::thaw() {
    ParamMapType map;
    deserialize(this->_packedParams, map);
    this->params = map;
}
/**
 * Serializes the routine default params
 */
void Routine::freeze() {
    BlobType bytes;
    serialize(this->params, bytes);
    this->_packedParams = bytes;
}



/**
 * Deserializes the group params
 */
void Group::thaw() {
    // deserialize state if it exists
    if(this->_packedState) {
        ParamMapType map;
        deserialize(*this->_packedState, map);
        this->routineState = std::make_unique<ParamMapType>(map);
    }
    // otherwise, clear it
    else {
        this->routineState = nullptr;
    }
}
/**
 * Serializes the group params
 */
void Group::freeze() {
    // serialize params if set
    if(this->routineState) {
        BlobType bytes;
        serialize(*this->routineState, bytes);
        this->_packedState = std::make_unique<BlobType>(bytes);
    }
    // otherwise, clear the blob
    else {
        this->_packedState = nullptr;
    }
}



/**
 * Deserializes the node UUID
 */
void Node::thaw() {
    uuids::uuid readUuid(this->_uuidBytes.begin(), this->_uuidBytes.end());
   
    if(readUuid.is_nil()) {
        auto dump = hexdump(this->_uuidBytes.begin(), this->_uuidBytes.end());
        auto what = f("Failed to decode node UUID from bytes '{}'", dump);
        throw std::runtime_error(what);
    }

    this->uuid = readUuid;
}

/**
 * Serializes the node UUID to bytes.
 */
void Node::freeze() {
    const auto uuidSpan = this->uuid.as_bytes();
    auto uuidBytes = reinterpret_cast<const char *>(uuidSpan.data());
    auto uuidLen = uuidSpan.size_bytes();

    this->_uuidBytes = std::vector(uuidBytes, uuidBytes+uuidLen);
}
