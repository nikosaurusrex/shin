CXX = clang++

ifeq ($(OS), Windows_NT)
	INCLUDES = -Iextern/include
	LIBS = -lextern/libs/freetype/freetype_static -lextern/libs/glfw/glfw3_mt -lextern/libs/glew/glew32s -lextern/libs/OpenGL32

	CXXFLAGS = -mno-stack-arg-probe -maes -fuse-ld=lld -D_CRT_SECURE_NO_WARNINGS $(INCLUDES)
	LDFLAGS = $(LIBS) -luser32 -lgdi32 -lshell32
	EXEC = build/shin.exe
else
	UNAME_S = $(shell uname -s)
	ifeq ($(UNAME_S), Darwin) #APPLE
		INCLUDES = `pkg-config --cflags glfw3`
		INCLUDES += `pkg-config --cflags freetype2`
		LIBS = `pkg-config --static --libs glfw3`
		LIBS += `pkg-config --static --libs freetype2`

		CXXFLAGS = $(INCLUDES)
		EXEC = build/shin
		LDFLAGS = $(LIBS) -framework OpenGL -framework Cocoa -framework IOKit
	endif
endif

CXXFLAGS += -O3 -std=c++20 -MMD

BUILD_DIR = build
IMGUI_DIR = extern/imgui
SRC_DIR = src

IMGUI_FILES = $(wildcard $(IMGUI_DIR)/*.cpp)
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(patsubst $(IMGUI_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(IMGUI_FILES))
OBJ_FILES += $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))
DEP_FILES = $(OBJ_FILES:.o=.d)

all: $(EXEC)

.PHONY: clean
clean:
	rm -f $(EXEC) imgui.ini $(OBJ_FILES) $(DEP_FILES)

$(BUILD_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(EXEC): $(OBJ_FILES)
	$(CXX) $(LDFLAGS) -o $(EXEC) $(OBJ_FILES) 

-include $(DEP_FILES)