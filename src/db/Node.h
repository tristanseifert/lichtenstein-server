/**
 * Defines the data store type representing nodes.
 */
#ifndef DB_NODE_H
#define DB_NODE_H

#include <sqlite3.h>

#include "json.hpp"

class DataStore;

class DbNode {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;

	friend void to_json(nlohmann::json& j, const DbNode& n);

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
		DbNode() {}

		/// converts a MAC address to a string
		static const std::string macToString(const uint8_t macIn[6]);

		/// return a string rendition of this node's MAC address
		inline const std::string macToString() const {
			return DbNode::macToString(this->macAddr);
		}

	private:
		inline DbNode(sqlite3_stmt *statement, DataStore *db) {
			this->_fromRow(statement, db);
		}

		void _create(DataStore *db);
		void _update(DataStore *db);

		void _fromRow(sqlite3_stmt *statement, DataStore *db);
		void _bindToStatement(sqlite3_stmt *statement, DataStore *db);

		static bool _macExists(uint8_t mac[6], DataStore *db);

	// operators
	friend bool operator==(const DbNode& lhs, const DbNode& rhs);
	friend bool operator< (const DbNode& lhs, const DbNode& rhs);
	friend std::ostream &operator<<(std::ostream& strm, const DbNode& obj);
};

#endif
