#include "Routine.h"

#include "Framebuffer.h"
#include <glog/logging.h>

#include <map>
#include <string>
#include <stdexcept>

#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>

using namespace std;

// declare some C functions
extern "C" void ASMessageCallback(const asSMessageInfo *msg, void *param);

/**
 * Initializes a new routine object with the given database routine (that's how
 * we get our Lua code) and properties to pass to that code.
 */
Routine::Routine(DataStore::Routine *r, map<string, double> &params) {
	this->routine = r;
	this->params = params;

	this->_setUpAngelscriptState();
}

Routine::Routine(DataStore::Routine *r) {
	this->routine = r;
	this->params = map<string, double>();

	this->_setUpAngelscriptState();
}

/**
 * Destroys the routine. This de-allocates the routine we were passed earlier.
 */
Routine::~Routine() {
	delete this->routine;

	// destroy old angelscript context
}

/**
 * Attaches the given buffer to this routine.
 */
void Routine::attachBuffer(vector<HSIPixel> *buf) {
	this->buffer = buf;
}

#pragma mark - AngelScript Stuff
/**
 * Sets up the AngelScript interpreter, loading the code from the routine stored
 * in the database.
 *
 * @note This throws an exception if the code couldn't be parsed/loaded.
 */
void Routine::_setUpAngelscriptState() {
	int err = 0;

	// create script context and register an error handler
	this->engine = asCreateScriptEngine();
	CHECK(this->engine != nullptr) << "Couldn't set up AngelScript context";

	// register message callback
	this->engine->SetMessageCallback(asFUNCTION(ASMessageCallback), 0, asCALL_CDECL);

	// register the std::string class as the string type
	RegisterStdString(this->engine);
}

/**
 * message handler for AngelScript - any messages given from the engine are just
 * printed to the log using the standard logging functions.
 */
extern "C" void ASMessageCallback(const asSMessageInfo *msg, void *param) {
	// format the message
	static const int msgBufSz = 4096;
	char msgBuf[msgBufSz];

	snprintf(msgBuf, msgBufSz, "AngelScript Message [section %s (%d:%d)] %s",
			 msg->section, msg->row, msg->col, msg->message);

	// log it
	if(msg->type == asMSGTYPE_ERROR) {
		LOG(ERROR) << msgBuf;
	} else if(msg->type == asMSGTYPE_WARNING) {
		LOG(WARNING) << msgBuf;
	} else if(msg->type == asMSGTYPE_INFORMATION) {
		LOG(INFO) << msgBuf;
	}
}

#pragma mark - Exceptions
/**
 * Creates a pretty error string (the "what" string) for this exception.
 */
void Routine::LoadError::_createWhatString() {
	switch(this->errCode) {

	}
}
