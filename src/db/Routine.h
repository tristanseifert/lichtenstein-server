/**
 * Defines the data store type representing routines.
 */
#ifndef DB_ROUTINE_H
#define DB_ROUTINE_H

#include <sqlite3.h>

#include <nlohmann/json.hpp>

class DataStore;

class DbRoutine {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;
	friend class OutputMapper;
	friend class Routine;

	friend class DbGroup;

	friend void to_json(nlohmann::json& j, const DbRoutine& n);

	private:
		int id = 0;

		std::string defaultParamsJSON;

	public:
		std::string name;

		std::string code;

		std::map<std::string, double> defaultParams;

	public:
    // default constructor to make a new routine
    DbRoutine() {}

    /**
     * Returns the id
     */
    inline int getId(void) {
      return this->id;
    }

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

inline std::ostream &operator<<(std::ostream& strm, const DbRoutine *obj) {
	strm << *obj;
	return strm;
}

#pragma mark - JSON Serialization
/**
 * Converts a routine object to a json representation.
 */
inline void to_json(nlohmann::json& j, const DbRoutine& routine) {
	// build the JSON representation
	j = nlohmann::json{
		{"id", routine.id},

		{"name", routine.name},
		{"code", routine.code},

		{"defaults", routine.defaultParams}
	};
}

inline void to_json(nlohmann::json& j, const DbRoutine *routine) {
	if(routine == nullptr) {
		j = nlohmann::json(nullptr);
	} else {
		j = nlohmann::json(*routine);
	}
}

#endif
