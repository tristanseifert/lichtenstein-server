#include "DataStorePrimitives.h"

#include <sstream>
#include <stdexcept>
#include <chrono>
#include <stdexcept>

#include "Logging.h"

#include <fmt/format.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <uuid.h>

// Cap'n Proto stuff 
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <db/packed_v1.capnp.h>

using namespace Lichtenstein::Server::DB::Types;
using namespace Lichtenstein::Server::DB::Types::IceChest;

/**
 * Deserializes a byte array back to an unordered map.
 */
static void deserialize(const std::vector<char> &bytes, ParamMapType &map) {
    using ValueType = RoutineParams::Entry::Value::Which;

    // create a message reader from the blob
    auto data = reinterpret_cast<const capnp::word*>(bytes.data());
    auto ptr = kj::arrayPtr(data, bytes.size());
    capnp::FlatArrayMessageReader reader(ptr);

    // iterate over every param in the packed message
    RoutineParams::Reader params = reader.getRoot<RoutineParams>();

    for(auto entry : params.getEntries()) {
        // get the value into the variant type in the map
        auto entryValue = entry.getValue();
        std::string key = entry.getKey();

        switch(entryValue.which()) {
            // Boolean
            case ValueType::BOOL:
                map.emplace(key, entryValue.getBool());
                break;
            // Floating point 
            case ValueType::FLOAT:
                map.emplace(key, entryValue.getFloat());
                break;
            // Unsigned integer
            case ValueType::UINT:
                map.emplace(key, entryValue.getUint());
                break;
            // Signed integer
            case ValueType::INT:
                map.emplace(key, entryValue.getInt());
                break;
            // Character string
            case ValueType::STRING:
                map.emplace(key, entryValue.getString());
                break;

            // unknown type
            default: {
                std::stringstream what;
                what << "Invalid type '" << (int)entryValue.which() 
                    << "' for key '" << key << "'";

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
    // create a memory buffer to serialize the message into
    capnp::MallocMessageBuilder writer;
    RoutineParams::Builder params = writer.initRoot<RoutineParams>();
    auto entries = params.initEntries(map.size());

    // iterate over each entry in the map
    int i = 0;
    for (auto [key, value] : map) {
        // copy over the key and value
        entries[i].setKey(key);
        auto entryValue = entries[i].initValue();

        if(std::holds_alternative<bool>(value)) {
            entryValue.setBool(std::get<bool>(value));
        }
        else if(std::holds_alternative<double>(value)) {
            entryValue.setFloat(std::get<double>(value));
        }
        else if(std::holds_alternative<uint64_t>(value)) {
            entryValue.setUint(std::get<uint64_t>(value));
        }
        else if(std::holds_alternative<int64_t>(value)) {
            entryValue.setInt(std::get<int64_t>(value));
        }
        else if(std::holds_alternative<std::string>(value)) {
            entryValue.setString(std::get<std::string>(value));
        }
        else {
            std::stringstream what;
            what << "Invalid type '" << value.index() << "' for key '" 
                << key << "''"; 

            throw std::invalid_argument(what.str());
        }
        // go to the next entry
        i++;
    }

    // update the blob
    auto data = capnp::messageToFlatArray(writer);
    auto dataBytes = data.asBytes();

    bytes.assign(dataBytes.begin(), dataBytes.end());

    Logging::debug("Serialized params into {} bytes", dataBytes.size());
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
        auto dump = spdlog::to_hex(this->_uuidBytes.begin(), this->_uuidBytes.end());
        auto what = fmt::format("Failed to decode node UUID from bytes '{}'", dump);
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
