#ifndef TRUETYPE_H
#define TRUETYPE_H

#include <ft2build.h>
#include FT_FREETYPE_H

class TrueType {
public:
    static void Init();
    static void Done();
    static FT_Library getLibrary() { return m_library; }
private:
    static FT_Library m_library;
};

#endif // TRUETYPE_H