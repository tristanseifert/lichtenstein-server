@0xc50dcc11b793bc27;

# Place our types into the right namespace
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("Lichtenstein::Server::DB::Types::IceChest");

#
# This is used to encode a routine's default parameters.
#
struct RoutineParams {
    entries     @0: List(Entry);

    struct Entry {
        key     @0: Text;
        value: union {
            bool        @1: Bool;
            float       @2: Float64;
            uint        @3: UInt64;
            int         @4: Int64;
            string      @5: Text;
            blob        @6: Data;
        }
    }
}
