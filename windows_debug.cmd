@echo off
pushd build

set CFLAGS=/nologo /EHsc /O2 /Z7 /std:c++20 /D_CRT_SECURE_NO_WARNINGS /I../extern/include 
set LDFLAGS=/OUT:shin_debug.exe /LIBPATH:../extern/libs/freetype /LIBPATH:../extern/libs/glfw /LIBPATH:../extern/libs/glew/ /LIBPATH:../extern/libs/
set LIBS=user32.lib gdi32.lib shell32.lib freetype_static.lib glfw3_mt.lib glew32s.lib OpenGL32.lib

set FILES=../extern/imgui/imgui.cpp ../extern/imgui/imgui_demo.cpp ../extern/imgui/imgui_draw.cpp ../extern/imgui/imgui_impl_glfw.cpp ../extern/imgui/imgui_impl_opengl3.cpp ../extern/imgui/imgui_tables.cpp ../extern/imgui/imgui_widgets.cpp ../src/buffer.cpp ../src/commands.cpp ../src/glyph_map.cpp ../src/highlighting.cpp ../src/renderer.cpp ../src/shin.cpp ../src/shortcuts.cpp

call cl %CFLAGS% %FILES% /link %LDFLAGS% %LIBS%

rm *.obj

popd