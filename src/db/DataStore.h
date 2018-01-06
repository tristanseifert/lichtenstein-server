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
#include <thread>
#include <mutex>

#include <ctime>

#include <sqlite3.h>

#include "INIReader.h"
#include "json.hpp"

#include "Group.h"
#include "Routine.h"
#include "Node.h"
#include "Channel.h"

// forward declare some classes the objects are friends with
class CommandServer;
class OutputMapper;

class DataStore {
	public:
		typedef void (*CustomFunction)(DataStore *, void *);

	public:
		DataStore(INIReader *reader);
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

	// types and functions relating to channels
	private:
		friend class DbChannel;

	public:
		std::vector<DbChannel *> getAllChannels();
		std::vector<DbChannel *> getChannelsForNode(DbNode *node);
		DbChannel *findChannelWithId(int id);

		void update(DbChannel *channel);

	// types and functions relating to routines
	private:
		friend class DbRoutine;

	public:
		std::vector<DbRoutine *> getAllRoutines();
		DbRoutine *findRoutineWithId(int id);

		void update(DbRoutine *routine);

	// types and functions relating to groups
	private:
		friend class DbGroup;

	public:
		std::vector<DbGroup *> getAllGroups();
		DbGroup *findGroupWithId(int id);

		void update(DbGroup *group);

	// types and functions relating to nodes
	private:
		friend class DbNode;

	public:
		std::vector<DbNode *> getAllNodes();
		DbNode *findNodeWithMac(uint8_t mac[6]);
		DbNode *findNodeWithId(int id);

		void update(DbNode *node);

	private:
		void open();
		void openConfigDb();

		void checkDbVersion();
		void provisonBlankDb();
		void updateStoredServerVersion();

		void upgradeSchema();

		void close();

	private:
		friend void BackgroundCheckpointThreadEntry(void *ctx);

		void createCheckpointThread();
		void _checkpointThreadEntry();

		void terminateCheckpointThread();

		std::thread *checkpointThread = nullptr;
		std::mutex checkpointLock;

	private:
		int sqlExec(const char *sql, char **errmsg);
		int sqlPrepare(const char *sql, sqlite3_stmt **stmt);

		int sqlBind(sqlite3_stmt *stmt, const char *param, std::string value, bool optional = false);
		int sqlBind(sqlite3_stmt *stmt, const char *param, void *data, int len, bool optional = false);
		int sqlBind(sqlite3_stmt *stmt, const char *param, int value, bool optional = false);

		int sqlStep(sqlite3_stmt *stmt);
		int sqlFinalize(sqlite3_stmt *stmt);

		int sqlGetLastRowId();

		int sqlGetNumColumns(sqlite3_stmt *statement);

		int sqlGetColumnInt(sqlite3_stmt *statement, int index);
		std::string sqlGetColumnString(sqlite3_stmt *statement, int index);
		const void *sqlGetColumnBlob(sqlite3_stmt *statement, int index, size_t &size);

		std::string sqlColumnName(sqlite3_stmt *statement, int index);

	private:
		std::string _stringFromColumn(sqlite3_stmt *statement, int col);

	private:
		INIReader *config;

		std::string path;
		sqlite3 *db;

		bool useDbLock = false;
		std::mutex dbLock;
};

#endif
