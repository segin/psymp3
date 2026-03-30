#ifndef TRUETYPE_H
#define TRUETYPE_H

// No direct includes - all includes should be in psymp3.h

class TrueType {
public:
    static void Init();

    /**
     * @brief Shuts down the global FreeType library instance, releasing all resources.
     *
     * Should only be called after all `Font` objects have been destroyed.
     */
    static void Done() { FT_Done_FreeType(m_library); }

    static FT_Library getLibrary() { return m_library; }
private:
    static FT_Library m_library;
};

#endif // TRUETYPE_H