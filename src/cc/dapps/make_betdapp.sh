#!/bin/bash

echo "Building betdapp"
g++-8 -c -std=c++17 -Wfatal-errors -Wall -Wextra -Werror subatomic_utils.cpp
gcc-8 -Wall -Wextra -Wfatal-errors -o ../../betdapp betdapp.c subatomic_utils.o -lstdc++ -lstdc++fs -lcurl -lm
echo "Build finished"