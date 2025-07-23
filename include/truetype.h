#ifndef TRUETYPE_H
#define TRUETYPE_H

// No direct includes - all includes should be in psymp3.h

class TrueType {
public:
    static void Init();
    static void Done();
    static FT_Library getLibrary() { return m_library; }
private:
    static FT_Library m_library;
};

#endif // TRUETYPE_H