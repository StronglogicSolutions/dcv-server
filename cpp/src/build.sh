#!/bin/bash
c++ -std=c++20 -I/dcv-channel/third_party/klogger/src linux_main.cpp link/context.cpp extensions.pb.cc /dcv-channel/third_party/klogger/src/logger.cpp /usr/lib/x86_64-linux-gnu/libprotobuf.a -lfmt -o dcv

