/**
 * Defines the data store type representing groups.
 *
 * A group's start and end refers to the location of its data in the master
 * framebuffer where its pixel data is written. Thus, groups can be used as an
 * "overlay" for one or more physical channels on nodes. This makes groups the
 * write counterpart to channels.
 */
#ifndef DB_GROUP_H
#define DB_GROUP_H

#include <sqlite3.h>

#include "json.hpp"

class DataStore;
class DbRoutine;

class DbGroup {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;
	friend class OutputMapper;

	friend void to_json(nlohmann::json& j, const DbGroup& n);

	private:
		int id = 0;
		int currentRoutineId = 0;

	public:
		std::string name;

		bool enabled;

		int start;
		int end;

		DbRoutine *currentRoutine;

	public:
		/**
		 * Returns the number of pixels this group encompasses.
		 */
		inline int numPixels() {
			return (this->end - this->start) + 1;
		}

    /**
     * Returns the id
     */
    inline int getId(void) {
      return this->id;
    }

	public:
    DbGroup() {}

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

inline std::ostream &operator<<(std::ostream& strm, const DbGroup *obj) {
	strm << *obj;
	return strm;
}

#pragma mark - JSON Serialization
/**
 * Converts a group object to a json representation.
 */
inline void to_json(nlohmann::json& j, const DbGroup& group) {
	// build the JSON representation
	j = nlohmann::json{
		{"id", group.id},

		{"name", group.name},

		{"enabled", group.enabled},

		{"start", group.start},
		{"end", group.end},

		{"currentRoutine", group.currentRoutine}
	};
}

inline void to_json(nlohmann::json& j, const DbGroup *group) {
	if(group == nullptr) {
		j = nlohmann::json(nullptr);
	} else {
		j = nlohmann::json(*group);
	}
}

#endif
