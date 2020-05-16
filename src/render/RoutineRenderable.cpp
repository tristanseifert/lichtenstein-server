#include "RoutineRenderable.h"
#include "HSIPixel.h"

#include <algorithm>
#include <random>

#include <angelscript.h>
#include <scriptstdstring/scriptstdstring.h>
#include <scriptbuilder/scriptbuilder.h>
#include <scriptarray/scriptarray.h>
#include <scriptdictionary/scriptdictionary.h>
#include <scriptmath/scriptmath.h>
#include <datetime/datetime.h>

#include "../Logging.h"

using namespace Lichtenstein::Server::Render;


/**
 * Initializes a routine renderable. This will compile the routine's code in
 * the and set up the script environment, throwing exceptions if needed.
 */
void RoutineRenderable::construct(size_t numPixels, const Routine &r, 
        const ParamMap &params) {
    // get the engine lock
    std::lock_guard engineLock(this->engineLock);
    
    // set up script context, then compile the routine
    this->initEngine();
    this->compileScript(r.code);
    this->createContext();
    
    // allocate a pixel buffer and set params
    this->resize(numPixels);

    this->updateScriptParams(params);
    this->params = params;
}
/**
 * Tears down the script engine and any other allocated resources.
 */
RoutineRenderable::~RoutineRenderable() {
    this->deinitEngine();
}



/**
 * Executes the script's `render()` function to generate output.
 */
void RoutineRenderable::render() {
    // run the render function while we hold the engine lock
    std::lock_guard engineLock(this->engineLock);
    this->executeRenderFxn();

    // copy data out of the intermediate buffer
    std::lock_guard bufLock(this->bufferLock);

    // miscellaneous accounting
    this->frameCounter++;
}

/**
 * Copies data out of our internal pixel buffer.
 */
void RoutineRenderable::copyOut(size_t offset, size_t num, 
        HSIPixel *out, bool mirrored) {
    // validate args
    XASSERT(out, "Output pointer cannot be null");
    
    XASSERT(offset < this->numPixels, "Offset must be in bounds");
    XASSERT((offset + num) <= this->numPixels, "Length must be in bounds");

    // perform copy
    std::lock_guard bufLock(this->bufferLock);

    if(mirrored) {
        // TODO: implement this
    } else {
        std::copy(this->buffer.begin(), this->buffer.end(), out);
    }

    Logging::trace("Avg exec time {}µS", this->getAvgExecutionTime());
}

/**
 * Resizes our internal buffer. If the size is smaller, pixels from the end
 * will be deleted. If it is larger than the current size, the new pixels are
 * undefined.
 */
void RoutineRenderable::resize(size_t numPixels) {
    // update script state
    if(this->asOutBuffer) {
        this->asOutBuffer->Release();
        this->asOutBuffer = nullptr;
    }
   
    HSIPixel fill(0,0,0);
    auto type = this->engine->GetTypeInfoByDecl("array<HSIPixel>");
    this->asOutBuffer = CScriptArray::Create(type, this->numPixels, &fill);

    Logging::trace("New out buffer for engine {} at {}", (void*)this, 
            (void*)this->asOutBuffer);

    // update our buffer size
    std::lock_guard bufLock(this->bufferLock);
    this->numPixels = numPixels;
    this->buffer.resize(numPixels, HSIPixel(0, 0, 0));
}



/**
 * Initializes the script engine.
 */
void RoutineRenderable::initEngine() {
    // create the engine and register message out callback
    this->engine = asCreateScriptEngine();
    if(!this->engine) {
        throw std::runtime_error("Failed to initialize AngelScript engine");
    }

    auto logFxn = asMETHOD(RoutineRenderable,engineMsg);
    this->engine->SetMessageCallback(logFxn, this, asCALL_THISCALL);

    // then, register AngelScript addons and init globals
    RegisterStdString(this->engine);
    RegisterScriptArray(this->engine, true);
    RegisterScriptDictionary(this->engine);
    RegisterScriptDateTime(this->engine);
    RegisterScriptMath(this->engine);

    this->initGlobals();

    // done!
    Logging::trace("Finished initialization for engine {}", 
            (void*)this->engine);
}
/**
 * Cleans up all engine resources.
 */
void RoutineRenderable::deinitEngine() {
    std::lock_guard engineLock(this->engineLock);

    // deallocate buffer if it exists
    if(this->asOutBuffer) {
        this->asOutBuffer->Release();
        this->asOutBuffer = nullptr;
    }
    
    // deallocate params dict
    if(this->asParamsDict) {
        this->asParamsDict->Release();
        this->asParamsDict = nullptr;
    }

    // clean up the script context and engine
    if(this->scriptCtx) {
        this->scriptCtx->Abort();

        this->scriptCtx->Unprepare();
        this->scriptCtx->Release();
        this->scriptCtx = nullptr;
    }

    if(this->engine) {
        this->engine->ShutDownAndRelease();
        this->engine = nullptr;
    }
}



/**
 * Compiles the given code into a script module; from this module, we look for
 * the `render()` function.
 */
void RoutineRenderable::compileScript(const std::string &code) {
    int err;
    CScriptBuilder builder;

    // create a script builder and load code into it
    err = builder.StartNewModule(this->engine, "UserScript");
    XASSERT(err == 0, "Failed to start script module");

    size_t scriptSz = code.size();
    const char *scriptCode = code.c_str();

    err = builder.AddSectionFromMemory("DefaultCode", scriptCode, scriptSz, 0);
    if(err != 1) {
        throw std::runtime_error("Failed to add user code to script module");
    }

    // finalize the module and extract the render function
    err = builder.BuildModule();
    if(err != 0) {
        throw std::runtime_error("Failed to build script module");
    }

    this->scriptMod = this->engine->GetModule("UserScript");
    XASSERT(this->scriptMod, "Script module is null after build");

    this->renderFxn = this->scriptMod->GetFunctionByDecl("void render()");
    if(!this->renderFxn) {
        throw std::invalid_argument("Code is missing render() function");
    }
}

/**
 * Creates a script context on which the script's render function can be
 * executed.
 */
void RoutineRenderable::createContext() {
    // allocate the context and prepare it to execute the render function
    this->scriptCtx = this->engine->CreateContext();
    this->scriptCtx->Prepare(this->renderFxn);

    // if(this->scriptCtx->GetState() == asEXECUTION_PREPARED) 
}



/**
 * Registers global variables.
 */
void RoutineRenderable::registerGlobalVars() {
    int err;
    auto e = this->engine;

    // number of pixels
    err = e->RegisterGlobalProperty("const uint64 numPixels", 
            &this->numPixels);
    XASSERT(err >= 0, "Couldn't register pixel count: err={}", err);

    // frame counter
    err = e->RegisterGlobalProperty("const uint64 frame", 
            &this->frameCounter);
    XASSERT(err >= 0, "Couldn't register pixel count: err={}", err);

    // register output buffer    
    err = e->RegisterGlobalProperty("array<HSIPixel> @buffer", 
            &this->asOutBuffer);
    XASSERT(err >= 0, "Couldn't register output buffer: err={}", err);

    // create and register routine params
    this->asParamsDict = CScriptDictionary::Create(this->engine);
    XASSERT(this->asParamsDict, "Failed to allocate dictionary");

    err = e->RegisterGlobalProperty("dictionary @properties", 
            &this->asParamsDict);
    XASSERT(err >= 0, "Couldn't register params dict: err={}", err);
}



/**
 * Registers global functions in the script engine.
 */
void RoutineRenderable::registerGlobalFunctions() {
    int err;
    auto e = this->engine;

    // register debug_print()
    err = e->RegisterGlobalFunction("void debug_print(const string &in)",
            asMETHOD(RoutineRenderable,scriptPrint), asCALL_THISCALL_ASGLOBAL, 
            this);
    XASSERT(err >= 0, "Couldn't register debug_print(): err={}", err);

    // register random_range()
    err = e->RegisterGlobalFunction("void random_range(int min, int max)",
            asMETHOD(RoutineRenderable,scriptRandom), asCALL_THISCALL_ASGLOBAL, 
            this);
    XASSERT(err >= 0, "Couldn't register random_range(): err={}", err);
}
/**
 * Logs an engine message.
 */
void RoutineRenderable::engineMsg(const asSMessageInfo *msg, void *param) {
    // format string
    const auto fmt = "Engine message ({}): [{} {:d}:{:d}] {}";

    // print it depending on its severity
    if(msg->type == asMSGTYPE_ERROR) {
        Logging::error(fmt, (void*)this, msg->section, msg->row, msg->col, 
                msg->message);
    } else if(msg->type == asMSGTYPE_WARNING) {
        Logging::warn(fmt, (void*)this, msg->section, msg->row, msg->col, 
                msg->message);
    } else if(msg->type == asMSGTYPE_INFORMATION) {
        Logging::info(fmt, (void*)this, msg->section, msg->row, msg->col, 
                msg->message);
    } 
}
/**
 * Prints a script message.
 */
void RoutineRenderable::scriptPrint(std::string &msg) {
    Logging::info("Script message ({}): {}", (void*)this, msg);
}
/**
 * Returns a random number in the given range.
 */
int RoutineRenderable::scriptRandom(int min, int max) {
    std::random_device dev;
    std::mt19937 random(dev());
    std::uniform_int_distribution<> dist(min, max);

    return dist(random);
}



/**
 * Registers the HSIPixel type.
 */
void RoutineRenderable::registerPixelType() {
    int err;
    auto e = this->engine;
    
    // register the base type
    err = e->RegisterObjectType("HSIPixel", sizeof(HSIPixel), 
            asOBJ_VALUE | asOBJ_POD);
    XASSERT(err >= 0, "Couldn't register HSIPixel type {}", err);

    // then its constructors and destructor
    err = e->RegisterObjectBehaviour("HSIPixel", asBEHAVE_CONSTRUCT, "void f()",
            asMETHOD(RoutineRenderable,hsiConstruct),
            asCALL_THISCALL_OBJLAST, this);
    XASSERT(err >= 0, "Couldn't register HSIPixel constructor {}", err);
    
    err = e->RegisterObjectBehaviour("HSIPixel", asBEHAVE_LIST_CONSTRUCT, 
            "void f(const int &in) {double, double, double}",
            asMETHOD(RoutineRenderable,hsiConstructList),
            asCALL_THISCALL_OBJLAST, this);
    XASSERT(err >= 0, "Couldn't register HSIPixel list constructor {}", err);
    
    err = e->RegisterObjectBehaviour("HSIPixel", asBEHAVE_DESTRUCT, "void f()",
            asMETHOD(RoutineRenderable,hsiDestruct),
            asCALL_THISCALL_OBJLAST, this);
    XASSERT(err >= 0, "Couldn't register HSIPixel destructor {}", err);

    // we also need the comparison and assignment operators
    err = e->RegisterObjectMethod("HSIPixel",
            "bool opEquals(const HSIPixel &in) const",
            asMETHODPR(HSIPixel, operator==,(const HSIPixel&) const, bool),
            asCALL_THISCALL, this);
    XASSERT(err >= 0, "Couldn't register HSIPixel compare operator {}", err);
    
    err = e->RegisterObjectMethod("HSIPixel",
            "HSIPixel &opAssign(const HSIPixel &in)",
            asMETHODPR(HSIPixel,operator =, (const HSIPixel &), HSIPixel&),
            asCALL_THISCALL, this);
    XASSERT(err >= 0, "Couldn't register HSIPixel assign operator {}", err);

    // also register the fields in the type
    err = this->engine->RegisterObjectProperty("HSIPixel", "double h",
            asOFFSET(HSIPixel, h));
    XASSERT(err >= 0, "Couldn't register HSIPixel.h: err={}", err);

    err = this->engine->RegisterObjectProperty("HSIPixel", "double s",
            asOFFSET(HSIPixel, s));
    XASSERT(err >= 0, "Couldn't register HSIPixel.s: err={}", err);
    
    err = this->engine->RegisterObjectProperty("HSIPixel", "double i",
            asOFFSET(HSIPixel, i));
    XASSERT(err >= 0, "Couldn't register HSIPixel.i: err={}", err);
}
/**
 * Allocates a new HSIPixel in the script engine.
 */
void RoutineRenderable::hsiConstruct(void *mem) {
    new(mem) HSIPixel();
}
/**
 * Allocates a new HSIPixel from a list of double values.
 */
void RoutineRenderable::hsiConstructList(double *list, void *mem) {
    new(mem) HSIPixel(list[0], list[1], list[2]);
}
/**
 * Deallocates a previously allocated HSIPixel.
 */
void RoutineRenderable::hsiDestruct(void *mem) {
    (static_cast<HSIPixel *>(mem))->~HSIPixel();
}



/**
 * Executes the rendering function.
 */
void RoutineRenderable::executeRenderFxn() {
    int err;

    // take timestamp and prepare for execution
    this->scriptStart = std::chrono::high_resolution_clock::now();

    // execute that shit
    this->scriptCtx->Prepare(this->renderFxn);
    err = this->scriptCtx->Execute();

    if(err != asEXECUTION_FINISHED) {
        Logging::warn("Failed to finish script execution for {}: err={}", 
                (void*)this, err);

        if(err == asEXECUTION_EXCEPTION) {
            int line, col;
            const char *sectionName;

            line = this->scriptCtx->GetExceptionLineNumber(&col, &sectionName);
            std::string what = fmt::format("Script {} exception in \"{}\" {:d}:{:d}) {}",
                    (void*)this, sectionName, line, col,
                    this->scriptCtx->GetExceptionString());
            throw std::runtime_error(what);
        }
    }

    // calculate time delay and copy buffers
    this->updateScriptExecTime();

    for(size_t i = 0; i < this->numPixels; i++) {
        auto *pix = static_cast<HSIPixel *>(this->asOutBuffer->At(i));
        this->buffer[i] = *pix;
    }
}

/**
 * Calculates how long the script took to execute, and updates the rolling
 * average counter.
 */
void RoutineRenderable::updateScriptExecTime() {
    using namespace std::chrono;

    auto now = high_resolution_clock::now();
    duration<double, std::micro> micros = (now - this->scriptStart);
    double execTime = micros.count();

    double n = this->avgExecutionTimeSamples;
    double oldAvg = this->avgExecutionTime;

    double newAvg = ((oldAvg * n) + execTime) / (n + 1);

    this->avgExecutionTime = newAvg;
    this->avgExecutionTimeSamples++;
}

/**
 * Updates the script dictionary with these new parameter values. This function
 * requires that the engine lock is held when it is invoked.
 */
void RoutineRenderable::updateScriptParams(const ParamMap &params) {
    XASSERT(this->asParamsDict, "No params dictionary allocated");

    // replace dictionary values
    this->asParamsDict->DeleteAll();

    for(const auto &[key, value] : params) {
        if(std::holds_alternative<bool>(value)) {
            auto b = std::get<bool>(value);
            this->asParamsDict->Set(key, &b, asTYPEID_BOOL);
        }
        else if(std::holds_alternative<double>(value)) {
            auto d = std::get<double>(value);
            this->asParamsDict->Set(key, d);
        }
        else if(std::holds_alternative<uint64_t>(value)) {
            auto num = std::get<uint64_t>(value);
            this->asParamsDict->Set(key, &num, asTYPEID_UINT64);
        }
        else if(std::holds_alternative<int64_t>(value)) {
            auto num = std::get<int64_t>(value);
            this->asParamsDict->Set(key, &num, asTYPEID_INT64);
        }
        else if(std::holds_alternative<std::string>(value)) {
            int strType = this->engine->GetTypeIdByDecl("string");
            const std::string str = std::get<std::string>(value);
            this->asParamsDict->Set(key, (void*)&str, strType);
        } else {
            auto what = fmt::format("Unable to convert param type {}", value.index());
            throw std::runtime_error(what);
        } 
    }
}

