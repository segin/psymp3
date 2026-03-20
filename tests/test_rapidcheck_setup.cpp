/*
 * test_rapidcheck_setup.cpp - RapidCheck setup verification
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_RAPIDCHECK
#include <rapidcheck.h>

int main() {
    std::cout << "Running RapidCheck setup test..." << std::endl;
    
    // Simple property to verify RapidCheck is linked and working
    bool success = rc::check("RapidCheck basic functionality", [](int a, int b) {
        return (a + b) == (b + a);
    });
    
    return success ? 0 : 1;
}
#else
int main() {
    std::cout << "RapidCheck not enabled, skipping test" << std::endl;
    return 0;
}
#endif
