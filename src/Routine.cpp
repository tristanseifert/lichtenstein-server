#include "Routine.h"

#include "Framebuffer.h"
#include <glog/logging.h>

#include <map>
#include <string>
#include <stdexcept>

extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

using namespace std;

/**
 * Initializes a new routine object with the given database routine (that's how
 * we get our Lua code) and properties to pass to that code.
 */
Routine::Routine(DataStore::Routine *r, map<string, double> &params) {
	this->routine = r;
	this->params = params;

	this->_setUpLuaState();
}

Routine::Routine(DataStore::Routine *r) {
	this->routine = r;
	this->params = map<string, double>();

	this->_setUpLuaState();
}

/**
 * Destroys the routine. This de-allocates the routine we were passed earlier.
 */
Routine::~Routine() {
	delete this->routine;

	// destroy old lua context
	if(this->luaState) {
		lua_close(this->luaState);
		this->luaState = nullptr;
	}
}

/**
 * Attaches the given buffer to this routine.
 */
void Routine::attachBuffer(vector<HSIPixel> *buf) {
	this->buffer = buf;
}

/**
 * Sets up the lua interpreter, loading the code from the routine stored in the
 * database.
 *
 * @note This throws an exception if the Lua code couldn't be parsed/loaded.
 */
void Routine::_setUpLuaState() {
	int err = 0;

	// destroy any old lua contexts
	if(this->luaState) {
		lua_close(this->luaState);
		this->luaState = nullptr;
	}

	// crate the lua context and load base libraries
	this->luaState = luaL_newstate();
	CHECK(this->luaState != nullptr) << "Couldn't create lua context";

	luaL_openlibs(this->luaState);

	// load in the string
/*	const char *luaCode = this->routine->lua.c_str();

	err = luaL_loadstring(this->luaState, luaCode);

	// process errors
	if(err != 0) {
		LOG(WARNING) << "Couldn't parse lua code for " << this->routine->name
					 << ": " << err;

		throw Routine::LoadError(err);
	}*/
}

#pragma mark Exceptions
/**
 * Creates a pretty error string (the "what" string) for this exception.
 */
void Routine::LoadError::_createWhatString() {
	switch(this->luaErrCode) {
		// syntax error; grab some detailed info from lua.
		case LUA_ERRSYNTAX: {
			// TODO: figure out how to get more detailed error messages
			snprintf(this->whatBuf, this->whatBufSz, "Syntax error: %i", this->luaErrCode);
			break;
		}

		// memory error
		case LUA_ERRMEM: {
			strncpy(this->whatBuf, "LUA_ERRMEM, out of memory", this->whatBufSz);
			break;
		}

		// unknown error
		default: {
			snprintf(this->whatBuf, this->whatBufSz, "Unknown lua error: %i", this->luaErrCode);
			break;
		}
	}
}
