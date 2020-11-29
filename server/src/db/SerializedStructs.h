/**
 * Various structs serialized into blobs in the database.
 */
#ifndef DB_SERIALIZEDSTRUCTS_H
#define DB_SERIALIZEDSTRUCTS_H

#include <cstdint>

#include <cista.h>

namespace Lichtenstein::Server::DB::Types::IceChest {
namespace data = cista::offset;

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Encoding/decoding modes used for these messages
 */
constexpr auto const kCistaMode = (cista::mode::WITH_VERSION | cista::mode::WITH_INTEGRITY |
                                   cista::mode::DEEP_CHECK);

///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Routine params structs
 */

/// Type of a routine parameter value
enum RoutineParamType {
    // the value part is not interpreted here
    TYPE_NULL,
    TYPE_BOOL,
    TYPE_UNSIGNED,
    TYPE_SIGNED,
    TYPE_FLOAT,
    TYPE_STRING
};

/// Multi-variable type
struct RoutineParamTypeContainer {
    RoutineParamType type = TYPE_NULL;

    struct {
        bool boolean;
        uint64_t numUnsigned;
        int64_t numSigned;
        double numFloat;
        data::string str;
    } v;
};

/// serialized param message
struct RoutineParams {
    data::hash_map<data::string, RoutineParamTypeContainer> params;
};


};

#endif
