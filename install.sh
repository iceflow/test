#!/bin/sh

make clean && $1 make && make install && touch BUILD_DONE
