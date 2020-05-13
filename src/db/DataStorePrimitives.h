/**
 * Defines the basic data-carrying structs that represent objects inside the
 * data store.
 */
#ifndef DB_DATASTOREPRIMITIVES_H
#define DB_DATASTOREPRIMITIVES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>

namespace Lichtenstein::Server::DB {
    class DataStore;
}

namespace Lichtenstein::Server::DB::Types {
    using BlobType = std::vector<char>;
    using ParamValueType = std::variant<bool,double,uint64_t,int64_t,std::string>;
    using ParamMapType = std::unordered_map<std::string, ParamValueType>; 

    struct BaseType {
        // last modified timestamp
        int lastModified;

        // sets the "last modified" timestamp to the current time
        virtual void updateLastModified();

        protected:
            friend class ::Lichtenstein::Server::DB::DataStore;
            // deserializes properties from blobs
            virtual void thaw() {};
            // re-serializes properties to blobs
            virtual void freeze() {};
    };

    /**
     * A single node that's previously connected to this server. Nodes can have
     * one or more output channels.
     */
    using NodeId = int;
    struct Node: public BaseType {
        NodeId id;

        std::shared_ptr<std::string> label;
        std::string address;
        std::string hostname;

        std::string swVersion;
        std::string hwVersion;

        // timestamp when the node last checked in
        int lastCheckin;
    };

    /**
     * Node output channel
     */
    using NodeChannelId = int;
    struct NodeChannel: public BaseType {
        NodeChannelId id;
        NodeId nodeId;

        std::shared_ptr<std::string> label;

        int nodeChannelIndex;
        int numPixels;
        int fbOffset;
        int format;
    };

    /**
     * Routines represent individual effects that can be run.
     */
    using RoutineId = int;
    struct Routine: public BaseType { 
        RoutineId id;
        std::string name;
        std::string code;

        // default parameters (in unserialized form)
        ParamMapType params;

        // default parameters (packed with cap'n proto™)
        std::vector<char> _packedParams;
        
        private:
            friend class ::Lichtenstein::Server::DB::DataStore;
            virtual void thaw();
            virtual void freeze();
    };

    /**
     * A single output group. This defines a region of our internal frame
     * buffer that can be individually controlled. It need not cover only a
     * single channel, or even node.
     */
    using GroupId = int;
    struct Group: public BaseType {
        GroupId id;
        std::string name;
        bool enabled = false;

        int startOff;
        int endOff;

        std::shared_ptr<RoutineId> routineId;
        // Routine state: may be NULL but only if routineId is also NULL
        std::shared_ptr<ParamMapType> routineState;

        double brightness = 1;

        // binary packed routine state (from cap'n proto)
        std::shared_ptr<BlobType> _packedState;
        
        private:
            friend class ::Lichtenstein::Server::DB::DataStore;
            virtual void thaw();
            virtual void freeze();
    };
}

#endif
