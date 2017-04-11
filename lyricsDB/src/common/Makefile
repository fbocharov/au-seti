BIN_DIR=../../bin
OBJ_DIR=./obj
SRC_DIR=.

AR=ar
AR_FLAGS=rcs
CXX=g++
CXX_FLAGS=-Wall -Werror -pedantic -g -std=c++14 -I../
LD_FLAGS=


SOURCES=$(wildcard $(SRC_DIR)/*.cpp)
OBJECTS_32=$(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.cpp=-32.o)))
OBJECTS_64=$(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.cpp=-64.o)))

all: filestructure libcommon32 libcommon64

filestructure:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%-32.o: $(SRC_DIR)/%.cpp
	$(CXX) -m32 -c $< $(CXX_FLAGS) -o $@

$(OBJ_DIR)/%-64.o: $(SRC_DIR)/%.cpp
	$(CXX) -m64 -c $< $(CXX_FLAGS) -o $@

clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

BIN_32=$(BIN_DIR)/libcommon32.a
libcommon32: $(OBJECTS_32)
	$(AR) $(AR_FLAGS) $(BIN_32) $(OBJECTS_32)

BIN_64=$(BIN_DIR)/libcommon64.a
libcommon64: $(OBJECTS_64)
	$(AR) $(AR_FLAGS) $(BIN_64) $(OBJECTS_64)

.PHONY: clean all