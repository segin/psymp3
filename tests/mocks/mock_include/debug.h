#ifndef MOCK_DEBUG_H
#define MOCK_DEBUG_H

#include <iostream>
#include <string>

class Debug {
public:
    template<typename... Args>
    static void log(const std::string& category, Args... args) {
        // Suppress logging for benchmark
    }
};

#endif
