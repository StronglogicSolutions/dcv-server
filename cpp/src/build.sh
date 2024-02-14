#!/bin/bash
c++ -std=c++20 -I/dcv-channel/third_party/klogger/src -I/dcv-channel/third_party/inih -I/dcv-channel/third_party/inih/cpp linux_main.cpp link/context.cpp link/socket/socket.cpp extensions.pb.cc /dcv-channel/third_party/klogger/src/logger.cpp /dcv-channel/third_party/inih/ini.c /dcv-channel/third_party/inih/cpp/INIReader.cpp /usr/lib/x86_64-linux-gnu/libprotobuf.a -lzmq -lfmt -o dcv
