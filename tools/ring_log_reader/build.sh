#!/usr/bin/env bash
set -e
g++ -O2 -std=c++17 -o ring_log_reader main.cpp
echo "BUILD_EXIT=$?"
