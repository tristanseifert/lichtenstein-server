/**
 * Provides JSON serialization support for database types.
 */
#ifndef DB_DATASTOREPRIMITIVES_JSON_H
#define DB_DATASTOREPRIMITIVES_JSON_H

#include <nlohmann/json_fwd.hpp>
#include "DataStorePrimitives.h"

using json = nlohmann::json;

namespace Lichtenstein::Server::DB::Types {
void to_json(json &, const Routine &);
void to_json(json &j, const Group &g);
void to_json(json &j, const Node &g);
void to_json(json &j, const NodeChannel &g);

void from_json(const json &, Routine &);
void from_json(const json &j, Group &g);
void from_json(const json &j, Node &g);
void from_json(const json &j, NodeChannel &g);
}

#endif
