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

class CScriptArray;

class Routine {
	public:
		// thrown if the script code can't be loaded
		class LoadError : public std::runtime_error {
			public:
				enum ErrorStage {
					kErrorStageNewModule = 1,
					kErrorStageBuildModule,
					kErrorStagePrepareContext
				};

			public:
				LoadError() = delete;
				LoadError(int code, ErrorStage stage) : std::runtime_error("script loading error") {
					this->errCode = code;
					this->stage = stage;

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

				int errCode;
				ErrorStage stage;
		};

	public:
		Routine() = delete;
		Routine(DataStore::Routine *r);
		Routine(DataStore::Routine *r, std::map<std::string, double> &params);
		~Routine();

		void attachBuffer(std::vector<HSIPixel> *buf);

		void execute(int frame);

	private:
		void _attachDebugger();

		void _cleanUpAngelscriptState();
		void _setUpAngelscriptState();

		void _updateASBufferArray();
		void _copyASBufferArrayData();

		void _setUpAngelscriptGlobals();

		asIScriptEngine *engine = nullptr;
		asIScriptContext *scriptCtx = nullptr;

	private:
		DataStore::Routine *routine;
		std::map<std::string, double> params;

		std::vector<HSIPixel> *buffer;

		int bufferSz = 0;
		CScriptArray *asBuffer = nullptr;

		int frameCounter = 0;
};

#endif
