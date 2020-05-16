/**
 * Compiles a routine's AngelScript code and executes it to produce pixel data
 * for each frame.
 */
#ifndef RENDER_ROUTINERENDERABLE_H
#define RENDER_ROUTINERENDERABLE_H

#include "IRenderable.h"

#include <mutex>
#include <vector>
#include <chrono>
#include <memory>

#include <angelscript.h>

#include "../db/DataStorePrimitives.h"

class CScriptArray;
class CScriptDictionary;

namespace Lichtenstein::Server::Render {
    class RoutineRenderable: public IRenderable {
        using Routine = Lichtenstein::Server::DB::Types::Routine;
        using ParamMap = decltype(Routine::params);
        using Timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>;

        public:
            RoutineRenderable(size_t numPixels) = delete;
            ~RoutineRenderable();
            
            /**
             * Constructs a renderable that renders the given routine, but with
             * the routine's default parameters.
             */
            RoutineRenderable(size_t numPixels, const Routine &inRoutine) : 
                    IRenderable(numPixels) {
                this->construct(numPixels, inRoutine, inRoutine.params);
            }
            /**
             * Constructs a renderable that renders the given routine using
             * the specified parameters.
             */
            RoutineRenderable(size_t numPixels, const Routine &inRoutine, 
                    const ParamMap &inParams) : IRenderable(numPixels) {
                this->construct(numPixels, inRoutine, inParams);
            }

            void render();
            void copyOut(size_t offset, size_t num, HSIPixel *out, bool mirrored = false);
            void resize(size_t numPixels);

            /**
             * Returns the average execution time of the script in µS
             */
            double getAvgExecutionTime() const {
                return this->avgExecutionTime;
            }

        public:
            /**
             * Updates the script's params dictionary
             */
            void setParams(const ParamMap &params) {
                std::lock_guard lg(this->engineLock);

                this->updateScriptParams(params);
                this->params = params;
            }

        private:
            void construct(size_t, const Routine &, const ParamMap &);

            void initEngine();
            void deinitEngine();

            void initGlobals() {
                this->registerGlobalFunctions();
                this->registerPixelType();
                this->registerGlobalVars();
            }
            void registerGlobalFunctions();
            void registerPixelType();
            void registerGlobalVars();

            void compileScript(const std::string &);
            void createContext();

            void executeRenderFxn();
            void updateScriptExecTime();

            void updateScriptParams(const ParamMap &);

        // helper functions called from script
        private:
            void engineMsg(const asSMessageInfo *, void *);
            void scriptPrint(std::string &);
            int scriptRandom(int, int);

        private:
            // output pixel buffer
            std::mutex bufferLock;
            std::vector<HSIPixel> buffer;

            // ID of the routine from which we were loaded
            int routineId = -1;

            // frame counter; this is incremented for every render() call
            uint64_t frameCounter = 0;

            // current parameters
            ParamMap params;
    
            // script engine state
            std::mutex engineLock;

            asIScriptEngine *engine = nullptr;
            asIScriptModule *scriptMod = nullptr;
            asIScriptFunction *renderFxn = nullptr;
            asIScriptContext *scriptCtx = nullptr;

            CScriptArray *asOutBuffer = nullptr;
            CScriptDictionary *asParamsDict = nullptr;

            // script execution time tracking
            double avgExecutionTime;
            double avgExecutionTimeSamples;

            Timestamp scriptStart;
    };
}

#endif
