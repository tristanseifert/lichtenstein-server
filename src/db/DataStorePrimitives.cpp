#include "DataStorePrimitives.h"

#include <sstream>
#include <stdexcept>

// Cap'n Proto stuff 
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <db/packed_v1.capnp.h>


using namespace Lichtenstein::Server::DB::Types;
using namespace Lichtenstein::Server::DB::Types::IceChest;

/**
 * Deserializes a byte array back to an unordered map.
 */
static void deserialize(const std::vector<char> &bytes, 
        std::unordered_map<std::string, std::any> &map) {
    using ValueType = RoutineParams::Entry::Value::Which;

    // create a message reader from the blob
    auto data = reinterpret_cast<const capnp::word*>(bytes.data());
    auto ptr = kj::arrayPtr(data, bytes.size());
    capnp::FlatArrayMessageReader reader(ptr);

    // iterate over every param in the packed message
    RoutineParams::Reader params = reader.getRoot<RoutineParams>();

    for(auto entry : params.getEntries()) {
        // massage the value into std::any
        std::any value;

        auto entryValue = entry.getValue();
        std::string key = entry.getKey();

        switch(entryValue.which()) {
            // Boolean
            case ValueType::BOOL:
                value.emplace<bool>(entryValue.getBool());
                break;
            // Floating point 
            case ValueType::FLOAT:
                value.emplace<double>(entryValue.getFloat());
                break;
            // Unsigned integer
            case ValueType::UINT:
                value.emplace<uint64_t>(entryValue.getUint());
                break;
            // Signed integer
            case ValueType::INT:
                value.emplace<bool>(entryValue.getInt());
                break;
            // Character string
            case ValueType::STRING:
                value.emplace<std::string>(entryValue.getString());
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

        // insert it into the map
        map[key] = value;
    }
}
/**
 * Serializes an unordered map into a byte array.
 */
static void serialize(const std::unordered_map<std::string, std::any> &map, 
        std::vector<char> &bytes) {
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

        if(value.type() == typeid(bool)) {
            entryValue.setBool(std::any_cast<bool>(value));
        }
        else if(value.type() == typeid(double)) {
            entryValue.setFloat(std::any_cast<double>(value));
        }
        else if(value.type() == typeid(uint64_t)) {
            entryValue.setUint(std::any_cast<uint64_t>(value));
        }
        else if(value.type() == typeid(int64_t)) {
            entryValue.setInt(std::any_cast<int64_t>(value));
        }
        else if(value.type() == typeid(std::string)) {
            entryValue.setString(std::any_cast<std::string>(value));
        }
        else {
            std::stringstream what;
            what << "Invalid type '" << value.type().name() << "' for key '" 
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

