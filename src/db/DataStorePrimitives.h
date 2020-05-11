/**
 * Defines the basic data-carrying structs that represent objects inside the
 * data store.
 */
#ifndef DB_DATASTOREPRIMITIVES_H
#define DB_DATASTOREPRIMITIVES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <any>
#include <memory>

namespace Lichtenstein::Server::DB {
    class DataStore;
}

namespace Lichtenstein::Server::DB::Types {
    using BlobType = std::vector<char>;
    using ParamMapType = std::unordered_map<std::string, std::any>; 

    struct BaseType {
        friend class Lichtenstein::Server::DB::DataStore;

        protected:
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

        std::unique_ptr<std::string> label;
        std::string address;
        std::string hostname;

        std::string swVersion;
        std::string hwVersion;

        // timestamp when the node last checked in
        int lastCheckin;
        // last time this node was modified
        int lastModified;
    };

    /**
     * Node output channel
     */
    using NodeChannelId = int;
    struct NodeChannel: public BaseType {
        NodeChannelId id;
        NodeId nodeId;

        std::unique_ptr<std::string> label;

        int nodeChannelIndex;
        int numPixels;
        int fbOffset;
        int format;

        // last time this channel was modified
        int lastModified;
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

        // last time this routine was modified
        int lastModified;

        // default parameters (packed with cap'n proto™)
        std::vector<char> _packedParams;
        
        protected:
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
        
        bool enabled;

        int startOff;
        int endOff;

        std::unique_ptr<RoutineId> routineId;
        // Routine state: may be NULL but only if routineId is also NULL
        std::unique_ptr<ParamMapType> routineState;

        double brightness;

        // last time this group was modified
        int lastModified;
        
        // binary packed routine state (from cap'n proto)
        std::unique_ptr<BlobType> _packedState;
        
        protected:
            virtual void thaw();
            virtual void freeze();
    };
}

#endif
