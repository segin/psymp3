#ifndef MOCK_TAGLIB_TSTRING_H
#define MOCK_TAGLIB_TSTRING_H

#include <string>
#include <cstring>

namespace TagLib {

class String {
public:
    String() {}
    String(const char* s) : m_str(s ? s : "") {}
    String(const std::string& s) : m_str(s) {}

    std::string to8Bit(bool utf8 = false) const { return m_str; }
    const char* toCString(bool utf8 = false) const { return m_str.c_str(); }
    const wchar_t* toCWString() const { return L""; } // Minimal mock
    bool isEmpty() const { return m_str.empty(); }

private:
    std::string m_str;
};

}

#endif
