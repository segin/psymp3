#include "psymp3.h"

FastFourier::FastFourier()
{
    m_handle = visual_dft_new(512, 512);
    if (!m_handle) {
        std::cerr << "visual_dft_new() failed! Exiting!" << std::endl;
        exit(1);
    }
}

FastFourier::~FastFourier()
{
    // XXX: not really sure what to do here, libvisual doesn't(?) provide a way to deallocate VisDFT *
}

void FastFourier::doFFT()
{
    int ret = visual_dft_perform(m_handle, m_fft, m_samples);
    if (ret != VISUAL_OK) {
        std::cerr << "visual_dft_perform() failed! Exiting!" << std::endl;
        exit(1);
    }
    // visual_dft_log_scale(m_fft, m_fft, 512);
}

void FastFourier::init()
{
    int ret = visual_init((int *) 0, (char ***) NULL);
    if (ret != VISUAL_OK) {
        std::cerr << "visual_init() failed! Exiting!" << std::endl;
        exit(1);
    }
}

