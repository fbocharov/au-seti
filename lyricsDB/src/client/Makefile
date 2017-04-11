BIN_DIR=../../bin
OBJ_DIR=./obj
SRC_DIR=.
BIN_32=$(BIN_DIR)/client32
BIN_64=$(BIN_DIR)/client64

CXX=g++
CXX_FLAGS=-Wall -Werror -pedantic -g -std=c++14 -I../
LD_FLAGS_32=-L$(BIN_DIR) -static -lnet32 -lprotocol32 -lcommon32
LD_FLAGS_64=-L$(BIN_DIR) -static -lnet64 -lprotocol64 -lcommon64

SOURCES=$(wildcard $(SRC_DIR)/*.cpp)
OBJECTS_32=$(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.cpp=-32.o)))
OBJECTS_64=$(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.cpp=-64.o)))

all: filestructure client32 client64

filestructure:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%-32.o: $(SRC_DIR)/%.cpp
	$(CXX) -m32 -c $< $(CXX_FLAGS) -o $@

$(OBJ_DIR)/%-64.o: $(SRC_DIR)/%.cpp
	$(CXX) -m64 -c $< $(CXX_FLAGS) -o $@

client32: $(OBJECTS_32)
	$(CXX) $(CXX_FLAGS) -o $(BIN_32) $(OBJECTS_32) $(LD_FLAGS_32) -m32

client64: $(OBJECTS_64)
	$(CXX) $(CXX_FLAGS) -o $(BIN_64) $(OBJECTS_64) $(LD_FLAGS_64) -m64

clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

.PHONY: clean all
