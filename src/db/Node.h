/**
 * Defines the data store type representing nodes.
 */
#ifndef DB_NODE_H
#define DB_NODE_H

#include <arpa/inet.h>
#include <sys/socket.h>

#include <sqlite3.h>

#include "json.hpp"

class DataStore;
class DbChannel;

class DbNode {
	// allow access to id field by command server for JSON serialization
	friend class DataStore;
	friend class CommandServer;

	friend class DbChannel;

	public:
		friend void to_json(nlohmann::json& j, const DbNode& n);

	private:
		int id = 0;

		/// configured channels, up to numChannels
		std::vector<DbChannel *> channels;

	public:
		uint32_t ip = 0;
		uint8_t macAddr[6];

		std::string hostname = "unknown";

		bool adopted = false;

		uint32_t hwVersion = 0;
		uint32_t swVersion = 0;

		time_t lastSeen = 0;

		/// total number of output channels
		int numChannels = 0;
		/// size of framebuffer, in bytes
		int fbSize = 0;

	public:
		// used to wait a certain number of packets if node is unrechable
		size_t errorTimer = 0;
		// packets with an error
		size_t errorPackets = 0;

	public:
		// Node() = delete;
		DbNode() {}
		~DbNode();

		/// converts a MAC address to a string
		static const std::string macToString(const uint8_t macIn[6]);

		/// return a string rendition of this node's MAC address
		inline const std::string macToString() const {
			return DbNode::macToString(this->macAddr);
		}

		/// returns a const reference to access the channels
		const std::vector<DbChannel *> &getChannels() const {
			return const_cast<const std::vector<DbChannel *> &>(this->channels);
		}

	private:
		DbNode(sqlite3_stmt *statement, DataStore *db);

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

inline std::ostream &operator<<(std::ostream& strm, const DbNode *obj) {
	strm << *obj;
	return strm;
}

#pragma mark - JSON Serialization
/**
 * Converts a node object to a json representation.
 */
inline void to_json(nlohmann::json& j, const DbNode& n) {
	// convert IP to a string
	static const int ipAddrSz = 24;
	char ipAddr[ipAddrSz];
	inet_ntop(AF_INET, &n.ip, ipAddr, ipAddrSz);

	// build the JSON representation
	j = nlohmann::json{
		{"id", n.id},

		{"mac", n.macToString()},
		{"ip", std::string(ipAddr)},
		{"hostname", n.hostname},

		{"adopted", n.adopted},

		{"hwVersion", n.hwVersion},
		{"swVersion", n.swVersion},

		{"lastSeen", n.lastSeen},

		{"numChannels", n.numChannels},
		{"fbSize", n.fbSize},

		{"channels", n.channels}
	};
}

inline void to_json(nlohmann::json& j, const DbNode *node) {
	if(node == nullptr) {
		j = nlohmann::json(nullptr);
	} else {
		j = nlohmann::json(*node);
	}
}

#endif
