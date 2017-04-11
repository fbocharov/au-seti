BIN_DIR=../../bin
OBJ_DIR=./obj
SRC_DIR=.
BIN=$(BIN_DIR)/server

CXX=g++
CXX_FLAGS=-Wall -Werror -pedantic -g -std=c++14 -I../
LD_FLAGS=-L$(BIN_DIR) -static -lnet64 -lprotocol64 -lcommon64 -pthread

SOURCES=$(wildcard $(SRC_DIR)/*.cpp)
OBJECTS=$(addprefix $(OBJ_DIR)/,$(notdir $(SOURCES:.cpp=.o)))

all: filestructure server

filestructure:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) -c $< $(CXX_FLAGS) -o $@

server: $(OBJECTS)
	$(CXX) $(CXX_FLAGS) -o $(BIN) $(OBJECTS) $(LD_FLAGS)

clean:
	rm -rf $(BIN_DIR)/* $(OBJ_DIR)/*

.PHONY: clean all
