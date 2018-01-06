/**
 * Defines the data store type representing channels.
 */
#ifndef DB_CHANNEL_H
#define DB_CHANNEL_H

#include <sqlite3.h>

#include "json.hpp"

class DataStore;
class DbNode;

class DbChannel {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;
	friend class OutputMapper;

	friend class DbNode;

	friend void to_json(nlohmann::json& j, const DbChannel& n);

	public:
		enum PixelFormat {
			kPixelFormatRGBW = 0,
			kPixelFormatRGB = 1
		};

	private:
		int id = 0;
		int nodeId = -1;

	public:
		/// which one of the node's channel numbers this corresponds to
		int nodeOffset;
		/// number of pixels attached to this channel
		int numPixels;
		/// at what offset in the framebuffer this channel takes its data
		int fbOffset;

		/// what format does the node expect data in for this channel?
		PixelFormat format;

		DbNode *node;

	public:
		// DbChannel() = delete;

	private:
		inline DbChannel(sqlite3_stmt *statement, DataStore *db, DbNode *node = nullptr) {
			// assign node if specified
			if(node) {
				this->node = node;
				this->nodeId = node->id;
			}


			this->_fromRow(statement, db);
		}

		void _create(DataStore *db);
		void _update(DataStore *db);

		void _fromRow(sqlite3_stmt *statement, DataStore *db);
		void _bindToStatement(sqlite3_stmt *statement, DataStore *db);

		static bool _idExists(int id, DataStore *db);

	// operators
	friend bool operator==(const DbChannel& lhs, const DbChannel& rhs);
	friend bool operator< (const DbChannel& lhs, const DbChannel& rhs);
	friend std::ostream &operator<<(std::ostream& strm, const DbChannel& obj);
};

inline std::ostream &operator<<(std::ostream& strm, const DbChannel *obj) {
	strm << *obj;
	return strm;
}

#pragma mark - JSON Serialization
/**
 * Converts a channel object to a json representation.
 */
inline void to_json(nlohmann::json& j, const DbChannel& channel) {
	// build the JSON representation
	j = nlohmann::json{
		{"id", channel.id},
		{"node", channel.nodeId},

		{"nodeIndex", channel.nodeOffset},

		{"size", channel.numPixels},
		{"fbOffset", channel.fbOffset}
	};
}

inline void to_json(nlohmann::json& j, const DbChannel *channel) {
	if(channel == nullptr) {
		j = nlohmann::json(nullptr);
	} else {
		j = nlohmann::json(*channel);
	}
}

#endif
