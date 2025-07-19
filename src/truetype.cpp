#include "psymp3.h"

FT_Library TrueType::m_library;

void TrueType::Init() {
    Debug::log("font", "TrueType::Init() called.");
    FT_Error error = FT_Init_FreeType(&m_library);
    if (error) {
        Debug::log("font", "TrueType::Init() failed: FT_Init_FreeType returned error code ", error);
        throw std::runtime_error("Failed to initialize FreeType2");
    }
    Debug::log("font", "TrueType::Init() successful.");
}

void TrueType::Done() {
    FT_Done_FreeType(m_library);
}

// RAII wrapper for TrueType initialization and cleanup.
class TrueTypeLifecycleManager {
public:
    TrueTypeLifecycleManager() {
        TrueType::Init();
    }
    ~TrueTypeLifecycleManager() {
        TrueType::Done();
    }
};

// The global static instance that manages the library's lifetime.
static TrueTypeLifecycleManager s_truetype_manager;