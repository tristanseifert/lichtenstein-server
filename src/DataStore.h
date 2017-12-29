/**
 * The data store is a simple database that's used to keep track of all state in
 * the server: the stored effect routines, available nodes, lighting groups, and
 * mapping the various sections of the framebuffer to output channels.
 */
#ifndef DATASTORE_H
#define DATASTORE_H

#include <string>

#include <sqlite3.h>

class DataStore {
	public:
		DataStore(std::string path);
		~DataStore();

		void commit();

		void optimize();

		void setInfoValue(std::string key, std::string value);
		std::string getInfoValue(std::string key);

	private:
		void open();
		void openConfigDb();
		void checkDbVersion();
		void provisonBlankDb();
		void updateStoredServerVersion();

		void close();

	private:
		std::string path;

		sqlite3 *db;
};

#endif
