#!/bin/sh

CFLAGS="-Wall -g -I lib/ -I third-party/ -std=c++20"
# LDFLAGS="-lz -lglfw -lGL"

LIB_SRCS=$(find lib third-party \
    -name '*.c' ! -name '*.windows.c')
LIBPP_SRCS=$(find lib third-party \
    -name '*.cc' ! -name '*.windows.cc')

# $CC \
#     $CFLAGS \
#     -o build/wz \
#     bin/wz.c \
#     $LIB_SRCS \
#     third-party/lodepng/lodepng.c \
#     $LDFLAGS
# 
# $CC \
#     $CFLAGS \
#     -o build/show \
#     bin/show.c \
#     $LIB_SRCS \
#     $LDFLAGS

$CC \
    $CFLAGS \
    -o build/show2 \
    bin/show2.cc \
    $LIBPP_SRCS \
    $LDFLAGS
