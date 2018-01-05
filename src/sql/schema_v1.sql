R"=====(
-- create the tables
CREATE TABLE routines (
	id integer PRIMARY KEY AUTOINCREMENT,
	name text,
	code text,
	defaultParams text DEFAULT '{}'
);

CREATE TABLE info (
	key text PRIMARY KEY,
	value text
);

CREATE TABLE nodes (
	id integer PRIMARY KEY AUTOINCREMENT,
	ip integer,
	mac blob,
	hostname text,
	adopted integer,
	hwversion integer,
	swversion integer,
	lastSeen datetime
);

CREATE TABLE groups (
	id integer PRIMARY KEY AUTOINCREMENT,
	name text,
	enabled integer,
	start integer,
	end integer,
	currentRoutine integer,

	-- enforce foreign keys
	FOREIGN KEY(currentRoutine) REFERENCES routines(id)
);

CREATE TABLE channels (
	id integer PRIMARY KEY AUTOINCREMENT,
	node integer,
	nodeOffset integer,
	numPixels integer,
	fbOffset integer,

	-- enforce foreign keys
	FOREIGN KEY(node) REFERENCES nodes(id)
);

-- create indices
CREATE UNIQUE INDEX IF NOT EXISTS idx_info_key ON info (key);

CREATE UNIQUE INDEX IF NOT EXISTS idx_node_id ON nodes (id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_node_mac ON nodes (mac);

CREATE UNIQUE INDEX IF NOT EXISTS idx_routines_id ON routines (id);
CREATE INDEX IF NOT EXISTS idx_routines_name ON routines (name);

CREATE UNIQUE INDEX IF NOT EXISTS idx_groups_id ON groups (id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_groups_name ON groups (name);

CREATE UNIQUE INDEX IF NOT EXISTS idx_channels_id ON channels (id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_channels_node ON channels (node);
CREATE UNIQUE INDEX IF NOT EXISTS idx_channels_node_data ON channels (node, id, numPixels, fbOffset);

-- insert default info values
-- INSERT INTO info (key, value) VALUES ("schema_version", "1");

-- INSERT INTO info (key, value) VALUES ("server_build", "unknown");
-- INSERT INTO info (key, value) VALUES ("server_version", "unknown");

-- )====="
