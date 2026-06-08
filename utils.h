#pragma once
#include <fstream>
#include <string>
#include <cstdio>

inline long get_mem_kb() {
    std::ifstream f("/proc/self/status");
    if (!f) return 0;
    std::string line;
    while (std::getline(f, line)) {
        long kb;
        if (sscanf(line.c_str(), "VmRSS: %ld kB", &kb) == 1) return kb;
    }
    return 0;
}
