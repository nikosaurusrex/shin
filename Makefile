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
		LDFLAGS = $(GLFW_LIBS) -framework OpenGL -framework Cocoa -framework IOKit
		EXEC = build/shin
		LDFLAGS = $(LIBS)
	endif
endif

CXXFLAGS += -g -Wall -std=c++20 

BUILD_DIR = build
IMGUI_DIR = extern/imgui

IMGUI_FILES = $(wildcard $(IMGUI_DIR)/*.cpp)
OBJ_FILES = $(patsubst $(IMGUI_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(IMGUI_FILES))

all: $(EXEC)

.PHONY: clean
clean:
	rm -f $(EXEC) build/imgui.ini $(OBJ_FILES)

$(BUILD_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(EXEC): $(OBJ_FILES) shin.cpp shin.h buffer.cpp commands.cpp shortcuts.cpp highlighting.cpp
	$(CXX) $(CXXFLAGS) -c -o build/shin.o shin.cpp
	$(CXX) $(LDFLAGS) -o $(EXEC) build/shin.o $(OBJ_FILES) 
