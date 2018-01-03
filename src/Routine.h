/**
 * Encapsulates the code for a particular routine, as well as its state. State
 * is stored as a key/value array that's accessible from within the Lua code.
 */
#ifndef ROUTINE_H
#define ROUTINE_H

#include "DataStore.h"
#include "Framebuffer.h"

#include <map>
#include <string>
#include <stdexcept>

// include lua for state n shit
extern "C" {
	#include "lua.h"
}

class Routine {
	public:
		// thrown if the lua code can't be loaded
		class LoadError : public std::runtime_error {
			public:
				LoadError() = delete;
				LoadError(int code) : std::runtime_error("lua loading error") {
					this->luaErrCode = code;
					this->_createWhatString();
				}

				virtual const char* what() const noexcept {
					return this->whatBuf;
				}

			private:
				void _createWhatString();

			private:
				static const int whatBufSz = 4096;
				char whatBuf[whatBufSz];

				int luaErrCode = 0;
		};

	public:
		Routine() = delete;
		Routine(DataStore::Routine *r);
		Routine(DataStore::Routine *r, std::map<std::string, double> &params);
		~Routine();

		void attachBuffer(std::vector<HSIPixel> *buf);

	private:
		void _setUpLuaState();

		lua_State *luaState = nullptr;

	private:
		DataStore::Routine *routine;
		std::map<std::string, double> params;

		std::vector<HSIPixel> *buffer;
};

#endif
