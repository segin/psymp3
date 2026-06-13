#include <iostream>
#include <string>
#include <chrono>

std::string urlEncodeOriginal(const std::string& input) {
    std::string result;
    for (unsigned char c : input) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '.' || c == '_' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
        }
    }
    return result;
}

std::string urlEncodeOptimized(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 3);
    for (unsigned char c : input) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '.' || c == '_' || c == '~') {
            result += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
        }
    }
    return result;
}

std::string urlEncodeSuperOptimized(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 3);

    // Use a lookup table to avoid multiple condition checks
    static const bool is_alnum_unreserved[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,  // '-' and '.' are true
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  // '0'-'9' are true
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 'A'-'O' are true
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  // 'P'-'Z', '_' are true
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 'a'-'o' are true
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,  // 'p'-'z', '~' are true
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static const char hex_chars[] = "0123456789ABCDEF";

    for (unsigned char c : input) {
        if (is_alnum_unreserved[c]) {
            result += c;
        } else {
            result += '%';
            result += hex_chars[c >> 4];
            result += hex_chars[c & 15];
        }
    }
    return result;
}

std::string urlEncodeHexOptimized(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 3);

    static const char hex_chars[] = "0123456789ABCDEF";

    for (unsigned char c : input) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '.' || c == '_' || c == '~') {
            result += c;
        } else {
            result += '%';
            result += hex_chars[c >> 4];
            result += hex_chars[c & 15];
        }
    }
    return result;
}

int main() {
    std::string testStr = "This is a test string that needs to be URL encoded! It has some special characters like: @, #, $, %, ^, &, *, (, ), +, =, [, ], {, }, |, \\, ;, :, ', \", <, >, ?, /, `.";

    // Repeat to make it longer
    for (int i = 0; i < 5; ++i) {
        testStr += testStr;
    }

    std::cout << "Test string length: " << testStr.length() << std::endl;

    const int iterations = 10000;

    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string res = urlEncodeOriginal(testStr);
        // prevent optimization
        if (res.empty()) std::cout << "empty" << std::endl;
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration1 = end1 - start1;

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string res = urlEncodeOptimized(testStr);
        // prevent optimization
        if (res.empty()) std::cout << "empty" << std::endl;
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration2 = end2 - start2;

    auto start3 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string res = urlEncodeSuperOptimized(testStr);
        // prevent optimization
        if (res.empty()) std::cout << "empty" << std::endl;
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration3 = end3 - start3;

    auto start4 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string res = urlEncodeHexOptimized(testStr);
        // prevent optimization
        if (res.empty()) std::cout << "empty" << std::endl;
    }
    auto end4 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration4 = end4 - start4;

    std::cout << "Original duration: " << duration1.count() << " ms\n";
    std::cout << "Optimized (reserve only) duration: " << duration2.count() << " ms\n";
    std::cout << "Optimized (reserve + hex chars) duration: " << duration4.count() << " ms\n";
    std::cout << "Super Optimized (reserve + lookup + hex chars) duration: " << duration3.count() << " ms\n";
    std::cout << "Improvement (reserve): " << ((duration1.count() - duration2.count()) / duration1.count()) * 100 << "%\n";
    std::cout << "Improvement (reserve + hex chars): " << ((duration1.count() - duration4.count()) / duration1.count()) * 100 << "%\n";
    std::cout << "Improvement (reserve + lookup + hex chars): " << ((duration1.count() - duration3.count()) / duration1.count()) * 100 << "%\n";

    return 0;
}
