#!/bin/bash
g++ -c src/mpris/MethodHandler.cpp -std=c++17 -DFINAL_BUILD -DHAVE_DBUS -Iinclude -Imock_dbus -Iinclude/mpris -Wall -Wextra -Werror -Wno-unused-parameter -o /dev/null
