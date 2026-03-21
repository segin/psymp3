#include "psymp3.h"
#ifdef HAVE_SPEEX
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#endif
int main() {
#ifdef HAVE_SPEEX
    speex_stereo_state_destroy(nullptr);
#endif
    return 0;
}
