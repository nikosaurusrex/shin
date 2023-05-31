cd build

CFLAGS="-g -Wall -std=c++20 "
CFLAGS+=`pkg-config --cflags --static --libs glfw3`
CFLAGS+=" -framework OpenGL "
CFLAGS+=`pkg-config --cflags --static --libs freetype2`

echo $CFLAGS
clang++ $CFLAGS -o shin ../shin.cpp

cd ../
