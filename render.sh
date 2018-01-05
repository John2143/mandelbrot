#!/bin/bash

if [[ -f outfile.bmp ]]; then
    FILENAME=`xxd -l 16 -g16 < /dev/urandom | cut -d" " -f2`
    convert outfile.bmp cool/$FILENAME.png
    rm outfile.bmp
else
    echo no file
fi
