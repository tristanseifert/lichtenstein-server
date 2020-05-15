/**
 * All data for the server is persisted in the data store, which is a thin
 * wrapper around an sqlite3 file.
 */
#ifndef DB_DATASTORE_H
#define DB_DATASTORE_H

#include <type_traits>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <algorithm>

#include <sqlite_orm.h>

#include "DataStorePrimitives.h"

namespace Lichtenstein::Server::DB {
    static inline auto initStorage(const std::string &path) {
        using namespace sqlite_orm;
        using namespace Lichtenstein::Server::DB::Types;

        return make_storage(path,
                make_table("routines",
                    make_column("id", &Routine::id, autoincrement(), primary_key()),
                    make_column("name", &Routine::name),
                    make_column("code", &Routine::code),
                    make_column("packedParams", &Routine::_packedParams),
                    make_column("lastModified", &Routine::lastModified)), 
                make_table("nodes",
                    make_column("id", &Node::id, autoincrement(), primary_key()),
                    make_column("label", &Node::label),
                    make_column("address", &Node::address),
                    make_column("hostname", &Node::hostname),
                    make_column("swVersion", &Node::swVersion),
                    make_column("hwVersion", &Node::hwVersion),
                    make_column("lastCheckin", &Node::lastCheckin),
                    make_column("lastModified", &Node::lastModified)),
                make_table("node_channels",
                    make_column("id", &NodeChannel::id, autoincrement(), primary_key()),
                    make_column("nodeId", &NodeChannel::nodeId),
                    make_column("label", &NodeChannel::label),
                    make_column("index", &NodeChannel::nodeChannelIndex),
                    make_column("numPixels", &NodeChannel::numPixels),
                    make_column("fbOffset", &NodeChannel::fbOffset),
                    make_column("format", &NodeChannel::format),
                    make_column("lastModified", &NodeChannel::lastModified),
                    foreign_key(&NodeChannel::nodeId).references(&Node::id)),
                make_table("groups",
                    make_column("id", &Group::id, autoincrement(), primary_key()),
                    make_column("name", &Group::name),
                    make_column("enabled", &Group::enabled),
                    make_column("mirrored", &Group::mirrored),
                    make_column("start", &Group::startOff),
                    make_column("end", &Group::endOff),
                    make_column("routineId", &Group::routineId),
                    make_column("routineState", &Group::_packedState),
                    make_column("brightness", &Group::brightness),
                    make_column("lastModified", &Group::lastModified),
                    foreign_key(&Group::routineId).references(&Routine::id)));
    }

    class DataStore {
        public:
            static void open();
            static void close();
            
            static std::shared_ptr<DataStore> db() {
                return sharedInstance;
            }

            // you should not call this!
            DataStore(const std::string &);
            virtual ~DataStore();

        public:

            /**
             * Returns a transaction goard; this works like std::lock_guard but
             * with database transactions. You need to call commit() before the
             * guard goes out of scope to make sure changes are saved.
             */
            auto getTransactionGuard() {
                return this->storage->transaction_guard();
            }

            /**
             * Runs the given lambda in a transaction. The return value of the
             * lambda decides whether we commit (true) or roll back (false) the
             * transaction.
             *
             * The function will return whether the data was committed or not.
             */
            bool transaction(std::function<bool()> f) {
                return this->storage->transaction(f);
            }

            /**
             * Reads a single row with the given id. If the object could not be
             * found, false is returned; otherwise, true.
             */
            template <class T> bool getOne(const int id, T &out) {
                static_assert(std::is_base_of<Types::BaseType, T>::value, 
                        "T must be one of the data store table types");
                try {
                    auto obj = this->storage->get<T>(id);
                    obj.thaw();

                    out = obj;
                    return true;
                } catch(std::system_error &e) {
                    // failed to find object
                    return false;
                }
            }

            /**
             * Gets all rows of a particular type.
             */
            template <class T> std::vector<T> getAll() {
                static_assert(std::is_base_of<Types::BaseType, T>::value, 
                        "T must be one of the data store table types");
                auto all = this->storage->get_all<T>();
                std::transform(all.begin(), all.end(), all.begin(), [](auto obj) {
                    obj.thaw();
                    return obj;
                });
                
                return all;
            }

            /**
             * Insert a new row into the data store. Its new ID is returned, as
             * well as written into the object.
             */
            template <class T> int insert(T &obj) {
                static_assert(std::is_base_of<Types::BaseType, T>::value, 
                        "T must be one of the data store table types");
                obj.freeze();
                obj.updateLastModified();

                int newId = this->storage->insert(obj);
                obj.id = newId;
                return newId;
            }
           
            /**
             * Updates an existing row in the data store. The row is located
             * by its id.
             */
            template <class T> void update(T &obj) {
                static_assert(std::is_base_of<Types::BaseType, T>::value, 
                        "T must be one of the data store table types");
                obj.freeze();
                
                this->storage->update(obj);
            }
            
            /**
             * Removes an existing row from the data store, identified by its
             * id column.
             */
            template <class T> void remove(const T &obj) {
                static_assert(std::is_base_of<Types::BaseType, T>::value, 
                        "T must be one of the data store table types");
                this->remove<T>(obj.id);
            }
            template <class T> void remove(const int id) {
                static_assert(std::is_base_of<Types::BaseType, T>::value, 
                        "T must be one of the data store table types");
                this->storage->remove<T>(id);
            }

        private:
            static std::shared_ptr<DataStore> sharedInstance;

        private:
            using Storage = decltype(initStorage(""));
            std::unique_ptr<Storage> storage;
    };
}

#endif
