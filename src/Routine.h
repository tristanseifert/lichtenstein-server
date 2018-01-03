/**
 * Encapsulates the code for a particular routine, as well as its state. State
 * is stored as a key/value array that's accessible from within the code.
 */
#ifndef ROUTINE_H
#define ROUTINE_H

#include "DataStore.h"
#include "Framebuffer.h"

#include <map>
#include <string>
#include <stdexcept>

#include <angelscript.h>

class Routine {
	public:
		// thrown if the script code can't be loaded
		class LoadError : public std::runtime_error {
			public:
				LoadError() = delete;
				LoadError(int code) : std::runtime_error("script loading error") {
					this->errCode = code;
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

				int errCode = 0;
		};

	public:
		Routine() = delete;
		Routine(DataStore::Routine *r);
		Routine(DataStore::Routine *r, std::map<std::string, double> &params);
		~Routine();

		void attachBuffer(std::vector<HSIPixel> *buf);

	private:
		void _setUpAngelscriptState();

		asIScriptEngine *engine;

	private:
		DataStore::Routine *routine;
		std::map<std::string, double> params;

		std::vector<HSIPixel> *buffer;
};

#endif
