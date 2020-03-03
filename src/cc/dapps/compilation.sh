#!/usr/bin/env bash

g++-8 -c -std=c++17 -Wfatal-errors -Wall -Wextra -Werror subatomic_utils.cpp
gcc-8 -Wall -Wextra -Wfatal-errors subatomic.c  subatomic_utils.o -lstdc++ -lstdc++fs -lcurl -lm
