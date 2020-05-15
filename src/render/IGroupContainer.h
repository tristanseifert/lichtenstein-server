/**
 * Implements an interface that allows the pipeline to query a render target
 * about its group memberships. Additionally, it can allow modification of
 * these group memberships.
 *
 * Groups are identified by their IDs, but data store objects can be passed as
 * well whose ID is used.
 */
#ifndef RENDER_IGROUPCONTAINER_H
#define RENDER_IGROUPCONTAINER_H

#include <cstddef>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

#include <spdlog/fmt/bundled/format.h>

#include "../db/DataStorePrimitives.h"

namespace Lichtenstein::Server::Render {
    class IGroupContainer {
        using Group = Lichtenstein::Server::DB::Types::Group;

        public:
            /**
             * Determines whether the container contains a group with the given
             * id.
             */
            virtual bool contains(int) const = 0;
            /**
             * Determines whether the container contains this group.
             */
            virtual bool contains(Group &g) const {
                return this->contains(g.id);
            }
            /**
             * Determines whether there is any intersection between the groups
             * in this container, and those in the passed in container.
             */
            virtual bool contains(IGroupContainer &rhs) const {
                std::vector<int> thisIds;
                std::vector<int> rhsIds;

                this->getGroupIds(thisIds);
                rhs.getGroupIds(rhsIds);

                return std::any_of(thisIds.begin(), thisIds.end(), [&](int id) {
                    return std::find(rhsIds.begin(), rhsIds.end(), id) != rhsIds.end();
                });
            }

            /**
             * Gets the union (intersection) of groups between this and another
             * container.
             */
            virtual void getUnion(const IGroupContainer &rhs, std::vector<int> &outIds) {
                // get IDs and sort them
                std::vector<int> thisIds;
                std::vector<int> rhsIds;

                this->getGroupIds(thisIds);
                std::sort(thisIds.begin(), thisIds.end());

                rhs.getGroupIds(rhsIds);
                std::sort(rhsIds.begin(), rhsIds.end());

                // calculate the intersection
                std::set_intersection(thisIds.begin(), thisIds.end(), 
                        rhsIds.begin(), rhsIds.end(), back_inserter(outIds));
            }

            /**
             * Total number of groups in this container.
             */
            virtual size_t numGroups() const = 0;

        public:
            /**
             * Whether the group container is mutable. By default, containers
             * are immutable. Implementations should override all mutation
             * functions if needed.
             */
            virtual bool isMutable() const {
                return false;
            }
            /**
             * Appends the given group to the end of the container.
             */
            virtual void appendGroup(const Group &g) {
                this->insertGroup(-1, g);
            }
            /**
             * Removes a group from the container, taking the id from the given
             * instance.
             */
            virtual void removeGroup(const Group &g) {
                this->removeGroup(g.id);
            }
            
            /**
             * Inserts a group at the specified position. Specifying an index
             * of -1 will append to the end of the container.
             */
            virtual void insertGroup(int, const Group &) {
                throw std::runtime_error("IGroupContainer::insertGroup() unimplemented");
            }

            /**
             * Removes the group with the given id from the container.
             */
            virtual void removeGroup(int) {
                throw std::runtime_error("IGroupContainer::removeGroup() unimplemented");
            }

        protected:
            /**
             * Gets the IDs of all groups in this container.
             */
            virtual void getGroupIds(std::vector<int> &) const = 0;
        
        public:
            /**
             * Checks two group containers for equality, e.g. whether they
             * contain the same groups.
             */
            inline bool operator==(const IGroupContainer &rhs) const noexcept {
                std::vector<int> thisIds;
                std::vector<int> rhsIds;

                this->getGroupIds(thisIds);
                rhs.getGroupIds(rhsIds);
              
                if(thisIds.size() != rhsIds.size()) {
                    return false;
                }

                // this ensures that order won't affect the result
                return std::all_of(thisIds.begin(), thisIds.end(), [&](int id) {
                    return std::find(rhsIds.begin(), rhsIds.end(), id) != rhsIds.end();
                });
            }

        friend std::ostream& operator<<(std::ostream &, const IGroupContainer &);
        friend struct fmt::formatter<IGroupContainer>;
    };

    inline std::ostream& operator<<(std::ostream &os, const IGroupContainer &c) {
        std::vector<int> ids;
        c.getGroupIds(ids);
        
        os << "(";
        for(auto it = ids.begin(); it != ids.end(); it++) {
            if(it != ids.begin()) os << ", ";
            os << *it;
        }
        os << ")";

        return os;
    }
}

template <>
struct fmt::formatter<Lichtenstein::Server::Render::IGroupContainer> {
    constexpr auto parse(format_parse_context &ctx) {
        auto it = ctx.begin(), end = ctx.end();

        if (it != end && *it != '}') {
            throw format_error("invalid format");
        }

        return it;
    }

    template <typename FormatContext>
    auto format(const Lichtenstein::Server::Render::IGroupContainer &c, 
            FormatContext &ctx) {
        std::vector<int> ids;
        c.getGroupIds(ids);

        std::stringstream text;
        text << c;

        return format_to(ctx.out(), "{}", text.str());
    }
};


#endif
