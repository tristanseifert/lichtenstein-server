/**
 * Defines the data store type representing groups.
 */
#ifndef DB_GROUP_H
#define DB_GROUP_H

#include <sqlite3.h>

#include "json.hpp"

class DataStore;

class DbGroup {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;
	friend class OutputMapper;

	friend void to_json(nlohmann::json& j, const DbGroup& n);

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
		// Group() = delete;

	private:
		inline DbGroup(sqlite3_stmt *statement, DataStore *db) {
			this->_fromRow(statement, db);
		}

		void _create(DataStore *db);
		void _update(DataStore *db);

		void _fromRow(sqlite3_stmt *statement, DataStore *db);
		void _bindToStatement(sqlite3_stmt *statement, DataStore *db);

		static bool _idExists(int id, DataStore *db);

	// operators
	friend bool operator==(const DbGroup& lhs, const DbGroup& rhs);
	friend bool operator< (const DbGroup& lhs, const DbGroup& rhs);
	friend std::ostream &operator<<(std::ostream& strm, const DbGroup& obj);
};

#endif
