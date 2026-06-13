#ifndef TRUETYPE_H
#define TRUETYPE_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3::Core {

class TrueType {
public:
    static void Init();

    /**
     * @brief Shuts down the global FreeType library instance, releasing all resources.
     *
     * Should only be called after all `Font` objects have been destroyed.
     */
    static void Done() { FT_Done_FreeType(m_library); }

    // Returns the global FreeType library, initializing it on first use. Init
    // runs at runtime (not during static initialization), so it has no cross-TU
    // static-order dependency and a FreeType init failure throws where a caller
    // can catch it rather than std::terminate before main().
    static FT_Library getLibrary();

private:
    static FT_Library m_library;
};

} // namespace PsyMP3::Core

#endif // TRUETYPE_H
