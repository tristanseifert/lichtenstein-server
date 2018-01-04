/**
 * Defines the data store type representing routines.
 */
#ifndef DB_ROUTINE_H
#define DB_ROUTINE_H

#include <sqlite3.h>

#include "json.hpp"

class DataStore;

class DbRoutine {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;
	friend class OutputMapper;
	friend class Routine;

	friend void to_json(nlohmann::json& j, const DbRoutine& n);

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
		inline DbRoutine(sqlite3_stmt *statement, DataStore *db) {
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
	friend bool operator==(const DbRoutine& lhs, const DbRoutine& rhs);
	friend bool operator< (const DbRoutine& lhs, const DbRoutine& rhs);
	friend std::ostream &operator<<(std::ostream& strm, const DbRoutine& obj);
};

#endif
