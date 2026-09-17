/*
 * G722Decoder.h - ITU-T G.722 sub-band ADPCM decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef G722DECODER_H
#define G722DECODER_H

namespace PsyMP3 {
namespace Codec {
namespace PCM {

/**
 * @brief ITU-T G.722 decoder: 7 kHz wideband speech in 64/56/48 kbit/s.
 *
 * G.722 splits the 16 kHz input into two 8 kHz sub-bands with a quadrature
 * mirror filter and codes each with its own ADPCM loop: six bits for the lower
 * band and two for the upper, packed into one octet with the upper band's bits
 * first. At 56 and 48 kbit/s the octet is the same; the decoder only ignores
 * the lower band's one or two least significant bits, which carry auxiliary
 * data instead (§1.3).
 * The decoder mirrors that: dequantise both bands, run each through a
 * two-pole/six-zero adaptive predictor, then recombine through the synthesis
 * QMF to get two output samples per octet.
 *
 * Written from the G.722 specification so PsyMP3 carries no external
 * dependency for it. The tables below are the normative ones from the
 * standard.
 */
class G722Decoder {
public:
    enum class Bitrate {
        Rate64k, ///< mode 1: all six lower-band bits are audio (the usual case)
        Rate56k, ///< mode 2: the lowest lower-band bit is auxiliary data
        Rate48k, ///< mode 3: the two lowest lower-band bits are auxiliary data
    };

    /// @param rate         the mode: how many lower-band bits are audio
    /// @param wideband_out true for the normal 16 kHz output; false selects the
    ///                     8 kHz mode, where the upper band is discarded and
    ///                     only the lower band's sample is emitted per octet.
    explicit G722Decoder(Bitrate rate = Bitrate::Rate64k, bool wideband_out = true);

    /// Discards all adaptive state, as at the start of a stream.
    void reset();

    /// Largest number of samples decode() can write for @p octets input.
    std::size_t maxSamples(std::size_t octets) const {
        return m_wideband ? octets * 2 : octets;
    }

    /// Decodes @p len octets into @p out, which must hold maxSamples(len)
    /// entries. Returns the number of samples written.
    std::size_t decode(const uint8_t* data, std::size_t len, int16_t* out);

private:
    /// One sub-band's ADPCM state: a two-pole, six-zero adaptive predictor
    /// plus its logarithmic scale factor.
    struct Band {
        int s = 0;        ///< predictor output
        int sp = 0;       ///< pole section output
        int sz = 0;       ///< zero section output
        int r[3] = {};    ///< reconstructed signal history
        int a[3] = {};    ///< pole coefficients
        int ap[3] = {};   ///< pole coefficients, updated
        int p[3] = {};    ///< partially reconstructed signal history
        int d[7] = {};    ///< quantised difference history
        int b[7] = {};    ///< zero coefficients
        int bp[7] = {};   ///< zero coefficients, updated
        int sg[7] = {};   ///< sign history driving the zero updates
        int nb = 0;       ///< logarithmic scale factor
        int det = 32;     ///< linear scale factor
    };

    /// Runs the predictor adaptation for one band given the dequantised
    /// difference used for adaptation, then recomputes its prediction.
    void adapt(Band& band, int dlt);

    Bitrate m_rate;
    bool m_wideband;
    Band m_low;
    Band m_high;
    int m_qmf[24] = {};      ///< synthesis QMF delay line, oldest first
};

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3

#endif // G722DECODER_H
