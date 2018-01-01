/**
 * The data store is a simple database that's used to keep track of all state in
 * the server: the stored effect routines, available nodes, lighting groups, and
 * mapping the various sections of the framebuffer to output channels.
 */
#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>
#include <vector>

#include <time.h>

#include <sqlite3.h>

class DataStore {
	public:
		DataStore(std::string path);
		~DataStore();

		void commit();

		void optimize();

	public:
		void setInfoValue(std::string key, std::string value);
		std::string getInfoValue(std::string key);

	// types and functions relating to nodes
	public:
		class Node {
			friend class DataStore;

			private:

			public:
				int id;

				uint32_t ip;
				uint8_t macAddr[6];

				std::string hostname;

				bool adopted;

				uint32_t hwVersion;
				uint32_t swVersion;

				time_t lastSeen;
		};
	public:
		DataStore::Node *findNodeWithMac(uint8_t mac[6]);
		void updateNode(DataStore::Node *node);

		std::vector<DataStore::Node *> getAllNodes();
	private:
		bool _nodeWithMacExists(uint8_t mac[6]);
		void _nodeFromRow(sqlite3_stmt *statement, DataStore::Node *node);
		void _bindNodeToStatement(sqlite3_stmt *statement, DataStore::Node *node);

		void _createNode(DataStore::Node *node);
		void _updateNode(DataStore::Node *node);

	private:
		void open();
		void openConfigDb();
		void checkDbVersion();
		void provisonBlankDb();
		void updateStoredServerVersion();

		void upgradeSchema();

		void close();

	private:
		std::string _stringFromColumn(sqlite3_stmt *statement, int col);

	private:
		std::string path;

		sqlite3 *db;
};

#endif
