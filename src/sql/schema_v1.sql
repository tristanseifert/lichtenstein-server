R"=====(
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

INSERT INTO info (key, value) VALUES ("schema_version", "1");

INSERT INTO info (key, value) VALUES ("server_build", "unknown");
INSERT INTO info (key, value) VALUES ("server_version", "unknown");
)====="
