/**
 * The data store is a simple database that's used to keep track of all state in
 * the server: the stored effect routines, available nodes, lighting groups, and
 * mapping the various sections of the framebuffer to output channels.
 */
#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>
#include <vector>
#include <iostream>
#include <map>

#include <time.h>

#include <sqlite3.h>

#include "json.hpp"

// forward declare some classes the objects are friends with
class CommandServer;
class OutputMapper;

class DataStore {
	public:
		typedef void (*CustomFunction)(DataStore *, void *);

	public:
		DataStore(std::string path);
		~DataStore();

		void commit();

		void optimize();

	public:
		void registerCustomFunction(std::string name, CustomFunction callback, void *ctx);
	private:
		friend void sqlFunctionHandler(sqlite3_context *, int, sqlite3_value **);

		class CustomFunctionCtx {
			friend class DataStore;
			friend void sqlFunctionHandler(sqlite3_context *, int, sqlite3_value **);

			private:
				std::string name;

				CustomFunction function;
				void *ctx;

				DataStore *ds;
		};

	public:
		void setInfoValue(std::string key, std::string value);
		std::string getInfoValue(std::string key);

	// types and functions relating to routines
	public:
		class Routine {
			// allow access to id field by command server for JSON serialization
			friend class DataStore;
			friend class CommandServer;
			friend class OutputMapper;
			friend class Routine;

			friend void to_json(nlohmann::json& j, const Routine& n);

			private:
				int id = 0;

				std::string defaultParamsJSON;

			public:
				std::string name;

				std::string code;

				std::map<std::string, double> defaultParams;

			public:
				// Routine() = delete;

			private:
				Routine(sqlite3_stmt *statement, DataStore *db) {
					this->_fromRow(statement, db);
				}

				void _decodeJSON();
				void _encodeJSON();

				void _create(DataStore *db);
				void _update(DataStore *db);

				void _fromRow(sqlite3_stmt *statement, DataStore *db);
				void _bindToStatement(sqlite3_stmt *statement, DataStore *db);

				static bool _idExists(int id, DataStore *db);

			// operators
			friend bool operator==(const Routine& lhs, const Routine& rhs);
			friend bool operator< (const Routine& lhs, const Routine& rhs);
			friend std::ostream &operator<<(std::ostream& strm, const Routine& obj);
		};
	public:
		std::vector<DataStore::Routine *> getAllRoutines();
		DataStore::Routine *findRoutineWithId(int id);

		void update(DataStore::Routine *routine);

	// types and functions relating to groups
	public:
		class Group {
			// allow access to id field by command server for JSON serialization
			friend class DataStore;
			friend class CommandServer;
			friend class OutputMapper;

			friend void to_json(nlohmann::json& j, const Group& n);

			private:
				int id = 0;

			public:
				std::string name;

				bool enabled;

				int start;
				int end;

				int currentRoutine;

			public:
				/**
				 * Returns the number of pixels this group encompasses.
				 */
				inline int numPixels() {
					return (this->end - this->start) + 1;
				}

			public:
				// Routine() = delete;

			private:
				Group(sqlite3_stmt *statement, DataStore *db) {
					this->_fromRow(statement, db);
				}

				void _create(DataStore *db);
				void _update(DataStore *db);

				void _fromRow(sqlite3_stmt *statement, DataStore *db);
				void _bindToStatement(sqlite3_stmt *statement, DataStore *db);

				static bool _idExists(int id, DataStore *db);

			// operators
			friend bool operator==(const Group& lhs, const Group& rhs);
			friend bool operator< (const Group& lhs, const Group& rhs);
			friend std::ostream &operator<<(std::ostream& strm, const Group& obj);
		};
	public:
		std::vector<DataStore::Group *> getAllGroups();
		DataStore::Group *findGroupWithId(int id);

		void update(DataStore::Group *group);

	// types and functions relating to nodes
	public:
		class Node {
			// allow access to id field by command server for JSON serialization
			friend class DataStore;
			friend class CommandServer;

			friend void to_json(nlohmann::json& j, const Node& n);

			private:
				int id = 0;

			public:
				uint32_t ip = 0;
				uint8_t macAddr[6];

				std::string hostname = "unknown";

				bool adopted = false;

				uint32_t hwVersion = 0;
				uint32_t swVersion = 0;

				time_t lastSeen = 0;

			public:
				// Node() = delete;
				Node() {}

				/// converts a MAC address to a string
				static const std::string macToString(const uint8_t macIn[6]);

				/// return a string rendition of this node's MAC address
				inline const std::string macToString() const {
					return Node::macToString(this->macAddr);
				}

			private:
				Node(sqlite3_stmt *statement, DataStore *db) {
					this->_fromRow(statement, db);
				}

				void _create(DataStore *db);
				void _update(DataStore *db);

				void _fromRow(sqlite3_stmt *statement, DataStore *db);
				void _bindToStatement(sqlite3_stmt *statement, DataStore *db);

				static bool _macExists(uint8_t mac[6], DataStore *db);

			// operators
			friend bool operator==(const Node& lhs, const Node& rhs);
			friend bool operator< (const Node& lhs, const Node& rhs);
			friend std::ostream &operator<<(std::ostream& strm, const Node& obj);
		};
	public:
		std::vector<DataStore::Node *> getAllNodes();
		DataStore::Node *findNodeWithMac(uint8_t mac[6]);

		void update(DataStore::Node *node);

	private:
		void open();
		void openConfigDb();
		void checkDbVersion();
		void provisonBlankDb();
		void updateStoredServerVersion();

		void upgradeSchema();

		void close();

	private:
		int sqlExec(const char *sql, char **errmsg);
		int sqlPrepare(const char *sql, sqlite3_stmt **stmt);

		int sqlBind(sqlite3_stmt *stmt, const char *param, std::string value, bool optional = false);
		int sqlBind(sqlite3_stmt *stmt, const char *param, void *data, int len, bool optional = false);
		int sqlBind(sqlite3_stmt *stmt, const char *param, int value, bool optional = false);

		int sqlStep(sqlite3_stmt *stmt);
		int sqlFinalize(sqlite3_stmt *stmt);

		int sqlGetLastRowId();

	private:
		std::string _stringFromColumn(sqlite3_stmt *statement, int col);

	private:
		std::string path;

		sqlite3 *db;
};

#endif
