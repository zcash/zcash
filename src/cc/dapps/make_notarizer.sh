#!/bin/bash

echo "Building notarizer"
mkdir -p ~/.dpow/bin 
g++-8 -c -std=c++17 -Wfatal-errors -Wall -Wextra -Werror subatomic_utils.cpp
gcc-8 -Wall -Wextra -Wfatal-errors -o ~/.dpow/bin/notarizer notarizer.c subatomic_utils.o -lstdc++ -lstdc++fs -lcurl -lm
echo "export PATH=$PATH:~/.dpow/bin/" >> ~/.bashrc # new session required to become effective
# run "export PATH=$PATH:~/.dpow/bin/" for immediate effect
echo "Build finished"