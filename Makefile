# name of the target binary
TARGET_EXEC ?= lichtenstein_server

# where the objects and source files go
BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

# flags for angelscript
ANGELSCRIPT_ADDONS := scriptbuilder scriptstdstring scriptarray scriptdictionary scriptmath datetime debugger
ANGELSCRIPT_ADDON_DIRS := $(addprefix libs/angelscript-sdk/add_on/,$(ANGELSCRIPT_ADDONS))
ANGELSCRIPT_ADDON_FILES := $(shell find $(ANGELSCRIPT_ADDON_DIRS) -name *.cpp -or -name *.c -or -name *.s)

ANGELSCRIPT_HEADERS := libs/angelscript-sdk/angelscript/include
ANGELSCRIPT_INC_DIRS := $(ANGELSCRIPT_HEADERS) libs/angelscript-sdk/add_on

ANGELSCRIPT_LIB_DIRS := libs/angelscript-sdk/angelscript/lib/
ANGELSCRIPT_LIB_NAME := libangelscript.a
ANGELSCRIPT_LIB_PATH := $(addprefix $(ANGELSCRIPT_LIB_DIRS),$(ANGELSCRIPT_LIB_NAME))

# input files
SRCS := $(shell find $(SRC_DIRS) -name *.cpp -or -name *.c -or -name *.s) $(ANGELSCRIPT_ADDON_FILES)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# libraries to link against
LIBS := stdc++ glog gflags sqlite3
LIBS_DIRS := $(ANGELSCRIPT_LIB_DIRS)

LIBS_FLAGS := $(addprefix -L,$(LIBS_DIRS)) $(addprefix -l,$(LIBS)) $(ANGELSCRIPT_LIB_PATH)

# directories to search for includes
INC_DIRS := $(shell find $(SRC_DIRS) -type d) libs/ libs/json/src libs/inih $(ANGELSCRIPT_INC_DIRS)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# flags for the C and C++ compiler
CPPFLAGS ?= -g $(INC_FLAGS) -MMD -MP -std=c++1z -fno-strict-aliasing
CFLAGS ?= -g -fno-strict-aliasing

# flags for the linker
LDFLAGS ?= -g $(LIBS_FLAGS)

# git version
GIT_HASH=`git rev-parse HEAD`
COMPILE_TIME=`date -u +'%Y-%m-%d %H:%M:%S UTC'`
GIT_BRANCH=`git branch | grep "^\*" | sed 's/^..//'`
export VERSION_FLAGS=-DGIT_HASH="\"$(GIT_HASH)\"" -DCOMPILE_TIME="\"$(COMPILE_TIME)\"" -DGIT_BRANCH="\"$(GIT_BRANCH)\"" -DVERSION="\"0.1.0\""

ifeq ($(BUILD),RELEASE)
	CFLAGS += -O2
	CPPFLAGS += -O2
else
	CFLAGS += -Og -DDEBUG="1"
	CPPFLAGS += -Og -DDEBUG="1"
endif

# all target
all: $(BUILD_DIR)/$(TARGET_EXEC)

# build the main executable
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(VERSION_FLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(VERSION_FLAGS) -c $< -o $@


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
