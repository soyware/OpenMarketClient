CXX=g++
CXXFLAGS=-std=c++17 -O2 -fno-unwind-tables -fno-asynchronous-unwind-tables -flto -Wall

# Directories
DIR_RAPIDJSON=../libs/rapidjson
DIR_INSTALLED_LIBS=/usr/local/lib
DIR_SRC=src
DIR_OUT=build/linux
DIR_OBJ=$(DIR_OUT)/obj

# Target
TARGET=OpenMarketClient

# Precompiled Header
PCH_HEADER=$(DIR_SRC)/Precompiled.h
PCH_SOURCE=$(DIR_SRC)/Precompiled.cpp
PCH_GCH=$(DIR_OBJ)/$(notdir $(PCH_HEADER)).gch

# Compiler and Linker Flags
LDFLAGS=-Wl,-s,-rpath,$(DIR_INSTALLED_LIBS) -lpthread -lstdc++fs -lwolfssl -lcurl
INCLUDES=-I$(DIR_RAPIDJSON)/include

# Find all .cpp files, excluding the PCH source which is not needed for compilation
SOURCES=$(filter-out $(PCH_SOURCE), $(shell find $(DIR_SRC) -name '*.cpp'))
# Create object file paths for regular sources
OBJECTS=$(patsubst $(DIR_SRC)/%.cpp,$(DIR_OBJ)/%.o,$(SOURCES))
# Create dependency file paths for object files
DEPS=$(patsubst %.o,%.d,$(OBJECTS))

.PHONY: all clean

all: $(DIR_OUT)/$(TARGET)

# Link the executable
$(DIR_OUT)/$(TARGET): $(OBJECTS)
	@echo "Linking..."
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile source files into object files
$(DIR_OBJ)/%.o: $(DIR_SRC)/%.cpp $(PCH_GCH) | $(DIR_OBJ)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -MF $(patsubst %.o,%.d,$@) -include $(PCH_HEADER) -c $< -o $@

# Create the precompiled header from the header file
$(PCH_GCH): $(PCH_HEADER) | $(DIR_OBJ)
	@echo "Creating precompiled header..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

# Create build directories
$(DIR_OBJ):
	@mkdir -p $(DIR_OBJ)

clean:
	@echo "Cleaning build files..."
	rm -rf $(DIR_OBJ) $(DIR_OUT)/$(TARGET)

# Include dependency files
-include $(DEPS)
