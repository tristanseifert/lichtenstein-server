R"=====(
-- create the tables
CREATE TABLE routines (
	id integer PRIMARY KEY AUTOINCREMENT,
	name text,
	lua text
);

CREATE TABLE info (
	key text,
	value text
);

CREATE TABLE nodes (
	id integer PRIMARY KEY AUTOINCREMENT,
	ip integer,
	mac integer,
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
	currentRoutine integer
);

CREATE TABLE channels (
	id integer PRIMARY KEY AUTOINCREMENT,
	node integer,
	numPixels integer,
	fbOffset integer
);

-- create indices
CREATE INDEX idx_info_key ON info (key);

CREATE UNIQUE INDEX idx_node_id ON nodes (id);
CREATE UNIQUE INDEX idx_node_mac ON nodes (mac);

CREATE UNIQUE INDEX idx_routines_id ON routines (id);
CREATE INDEX idx_routines_name ON routines (name);

CREATE UNIQUE INDEX idx_groups_id ON groups (id);
CREATE UNIQUE INDEX idx_groups_name ON groups (name);

CREATE UNIQUE INDEX idx_channels_id ON channels (id);
CREATE UNIQUE INDEX idx_channels_node ON channels (node);
CREATE UNIQUE INDEX idx_channels_node_data ON channels (node, id, numPixels, fbOffset);

-- insert default info values
INSERT INTO info (key, value) VALUES ("schema_version", "1");

INSERT INTO info (key, value) VALUES ("server_build", "unknown");
INSERT INTO info (key, value) VALUES ("server_version", "unknown");

-- )====="
