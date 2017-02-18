#!/bin/bash
cp -p ../cruiser.ino cruiser.cpp && cp -p ../map.h . && cp -p ../sprites.h . && gcc -g -o cruiser -I. cruiser.cpp main.cpp Gamebuino.cpp -lm -lGL -lGLU -lglut

