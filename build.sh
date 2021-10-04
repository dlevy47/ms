#!/bin/sh

CFLAGS="-Wall -g -I lib/ -I third-party/"
LDFLAGS="-lz -lglfw -lGL"

LIB_SRCS=$(ls lib/wz/*.c lib/wz-cache/*.c)

clang \
    $CFLAGS \
    -o build/wz \
    bin/wz.c \
    $LIB_SRCS \
    third-party/lodepng/lodepng.c \
    $LDFLAGS

clang \
    $CFLAGS \
    -o build/show \
    bin/show.c \
    $LIB_SRCS \
    $LDFLAGS
