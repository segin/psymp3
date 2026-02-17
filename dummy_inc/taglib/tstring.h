#pragma once
#include <string>
#include <iostream>
namespace TagLib {
    using FileName = const char*;
    class ByteVector {
    public:
        ByteVector() {}
        ByteVector(const char*, unsigned int) {}
        ByteVector(unsigned int, char) {}
        unsigned int size() const { return 0; }
        const char* data() const { return nullptr; }
        bool isEmpty() const { return true; }
        bool operator==(const ByteVector&) const { return true; }
    };
    class String {
    public:
        String() {}
        String(const char*) {}
        String(const std::string&) {}
        std::string to8Bit(bool=false) const { return ""; }
        bool operator==(const String&) const { return true; }
        bool operator!=(const String&) const { return false; }
        bool isEmpty() const { return true; }
        friend std::ostream& operator<<(std::ostream& os, const String&) { return os; }
    };
}
