/*
 * mlp_decoder.cpp - Meridian Lossless Packing / Dolby TrueHD decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Unlike the rest of PsyMP3, which is ISC, this file is Apache-2.0: it is a
 * derived work of an Apache-2.0 reference implementation and carries that
 * licence forward.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This is an independent C++ implementation of the MLP/TrueHD bitstream, written
 * against truehdd <https://github.com/truehdd/truehdd> as its reference for the
 * format. truehdd is Copyright © the truehdd contributors and is licensed under
 * the Apache License, Version 2.0.
 *
 * Verified bit-exact against an ffmpeg reference decode of a 48 kHz stereo
 * TrueHD stream: 308,454 access units, 24,676,310 samples, no differences.
 */

#include "mlp_decoder.h"

#include <cstring>

namespace mlp {

namespace {

constexpr size_t MAX_CHANNELS = 16;
constexpr size_t MAX_BLOCK_SIZE = 160;
constexpr size_t MAX_SUBSTREAMS = 4;

/// Blocks per substream segment. The syntax does not bound this directly; the
/// count is capped so a corrupt stream cannot spin here.
constexpr size_t MAX_BLOCKS_PER_SEGMENT = 8;

// ---------------------------------------------------------------- bit reader

/// Big-endian bit reader over one access unit.
class BitReader {
public:
    BitReader(const uint8_t* data, size_t size)
        : m_data(data), m_bits(static_cast<uint64_t>(size) * 8), m_pos(0) {}

    bool getBit()
    {
        if (m_pos >= m_bits) {
            throw Error("bit reader ran past the end of the access unit");
        }
        bool bit = (m_data[m_pos >> 3] >> (7 - (m_pos & 7))) & 1;
        ++m_pos;
        return bit;
    }

    uint32_t get(unsigned n)
    {
        if (n == 0) {
            return 0;
        }
        if (m_pos + n > m_bits) {
            throw Error("bit reader ran past the end of the access unit");
        }
        uint32_t value = 0;
        for (unsigned i = 0; i < n; ++i) {
            value = (value << 1) | ((m_data[m_pos >> 3] >> (7 - (m_pos & 7))) & 1);
            ++m_pos;
        }
        return value;
    }

    /// Two's-complement read: the top bit of the field is the sign.
    int32_t getSigned(unsigned n)
    {
        if (n == 0) {
            return 0;
        }
        uint32_t raw = get(n);
        uint32_t sign = 1u << (n - 1);
        return static_cast<int32_t>((raw ^ sign) - sign);
    }

    void skip(unsigned n)
    {
        if (m_pos + n > m_bits) {
            throw Error("bit reader ran past the end of the access unit");
        }
        m_pos += n;
    }

    void seek(int64_t delta)
    {
        int64_t target = static_cast<int64_t>(m_pos) + delta;
        if (target < 0 || static_cast<uint64_t>(target) > m_bits) {
            throw Error("bit reader seek out of range");
        }
        m_pos = static_cast<uint64_t>(target);
    }

    void seekTo(uint64_t bit)
    {
        if (bit > m_bits) {
            throw Error("bit reader seek out of range");
        }
        m_pos = bit;
    }

    uint64_t position() const { return m_pos; }
    uint64_t available() const { return m_bits - m_pos; }
    size_t sizeBytes() const { return static_cast<size_t>(m_bits / 8); }
    const uint8_t* data() const { return m_data; }

    /// Advances to the next 16-bit boundary. Substream segments are word aligned.
    void align16()
    {
        if (m_pos & 7) {
            m_pos += 8 - (m_pos & 7);
        }
        if (m_pos & 15) {
            m_pos += 8;
        }
        if (m_pos > m_bits) {
            throw Error("alignment ran past the end of the access unit");
        }
    }

    /// XOR of the bytes spanning the last `len` bits.
    uint8_t parityByte(uint64_t len) const
    {
        uint8_t parity = 0;
        uint64_t start = m_pos - len;
        for (uint64_t i = 0; i < len; i += 8) {
            parity ^= m_data[(start + i) >> 3];
        }
        return parity;
    }

    /// The same, folded to a nibble, as the access unit header check uses.
    uint8_t parityNibble(uint64_t len) const
    {
        uint8_t parity = parityByte(len);
        parity ^= parity >> 4;
        return parity & 0xF;
    }

private:
    const uint8_t* m_data;
    uint64_t m_bits;
    uint64_t m_pos;
};

// ---------------------------------------------------------------------- CRC

/// Advances a CRC register by `len` zero bits.
uint8_t crc8Bits(uint8_t poly, uint8_t value, unsigned len)
{
    for (unsigned i = 0; i < len; ++i) {
        value = static_cast<uint8_t>((value << 1) ^ (((value >> 7) & 1) * poly));
    }
    return value;
}

struct Crc8Table {
    uint8_t poly;
    uint8_t init;
    uint8_t table[256];

    Crc8Table(uint8_t p, uint8_t i) : poly(p), init(i)
    {
        for (int n = 0; n < 256; ++n) {
            table[n] = crc8Bits(poly, static_cast<uint8_t>(n), 8);
        }
    }
};

const Crc8Table& restartHeaderCrc()
{
    static const Crc8Table crc(0x1D, 0x00);
    return crc;
}

const Crc8Table& substreamCrc()
{
    static const Crc8Table crc(0x63, 0xA2);
    return crc;
}

/// CRC over a bit range that need not start or end on a byte boundary.
///
/// The register is advanced over the field and the value XORed in afterwards, so
/// a whole byte is table[crc] ^ byte rather than the more familiar
/// table[crc ^ byte]. The bytes between the leading and trailing partial fields
/// are not byte aligned either, so they are read through the bit reader.
uint8_t crc8Range(const Crc8Table& crc, const uint8_t* data, size_t size,
                  uint64_t start, uint64_t len)
{
    uint8_t checksum = crc.init;
    uint64_t prefix_len = start & 7;
    uint64_t suffix_len = (len - prefix_len) & 7;
    uint64_t middle_len = len - prefix_len - suffix_len;

    BitReader reader(data, size);
    reader.seekTo(start);

    if (prefix_len != 0) {
        uint8_t prefix = static_cast<uint8_t>(reader.get(static_cast<unsigned>(prefix_len)));
        checksum = static_cast<uint8_t>(
            crc8Bits(crc.poly, checksum, static_cast<unsigned>(prefix_len)) ^ prefix);
    }

    for (uint64_t i = 0; i < middle_len; i += 8) {
        checksum = static_cast<uint8_t>(crc.table[checksum] ^ static_cast<uint8_t>(reader.get(8)));
    }

    if (suffix_len != 0) {
        uint8_t suffix = static_cast<uint8_t>(reader.get(static_cast<unsigned>(suffix_len)));
        checksum = static_cast<uint8_t>(
            crc8Bits(crc.poly, checksum, static_cast<unsigned>(suffix_len)) ^ suffix);
    }

    return checksum;
}

// ------------------------------------------------------------------ Huffman

/// One residual code: its bit pattern, its width, and the value it stands for.
struct HuffEntry {
    uint16_t code;
    uint8_t bits;
    int8_t value;
};

/// The negative half, which all three tables share. The deepest value is reached
/// through a nine-bit code whose last bit the table ignores.
const HuffEntry HUFF_NEGATIVE[] = {
    {0x1, 3, -1},   // 001
    {0x1, 4, -2},   // 0001
    {0x1, 5, -3},   // 00001
    {0x1, 6, -4},   // 000001
    {0x1, 7, -5},   // 0000001
    {0x1, 8, -6},   // 00000001
    {0x0, 9, -7},   // 000000000
    {0x1, 9, -7},   // 000000001
};

const HuffEntry HUFF1[] = {
    {0x4, 3, 0}, {0x5, 3, 1}, {0x6, 3, 2}, {0x7, 3, 3},   // 100 101 110 111
    {0x3, 3, 4}, {0x5, 4, 5}, {0x9, 5, 6}, {0x11, 6, 7},  // 011 0101 01001 010001
    {0x21, 7, 8}, {0x41, 8, 9}, {0x80, 9, 10}, {0x81, 9, 10},
};

const HuffEntry HUFF2[] = {
    {0x2, 2, 0}, {0x3, 2, 1},                             // 10 11
    {0x3, 3, 2}, {0x5, 4, 3}, {0x9, 5, 4}, {0x11, 6, 5},
    {0x21, 7, 6}, {0x41, 8, 7}, {0x80, 9, 8}, {0x81, 9, 8},
};

const HuffEntry HUFF3[] = {
    {0x1, 1, 0},                                          // 1
    {0x3, 3, 1}, {0x5, 4, 2}, {0x9, 5, 3}, {0x11, 6, 4},
    {0x21, 7, 5}, {0x41, 8, 6}, {0x80, 9, 7}, {0x81, 9, 7},
};

/// Every code is at most nine bits, so one flat lookup decodes any of them.
struct HuffLut {
    int8_t value[512];
    uint8_t bits[512];
};

void fillLut(HuffLut& lut, const HuffEntry* entries, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const HuffEntry& e = entries[i];
        unsigned pad = 9u - e.bits;
        unsigned base = static_cast<unsigned>(e.code) << pad;
        for (unsigned n = 0; n < (1u << pad); ++n) {
            lut.value[base + n] = e.value;
            lut.bits[base + n] = e.bits;
        }
    }
}

const HuffLut* huffTables()
{
    static HuffLut tables[3];
    static bool built = false;
    if (!built) {
        const HuffEntry* sets[3] = {HUFF1, HUFF2, HUFF3};
        const size_t counts[3] = {sizeof(HUFF1) / sizeof(*HUFF1),
                                  sizeof(HUFF2) / sizeof(*HUFF2),
                                  sizeof(HUFF3) / sizeof(*HUFF3)};
        for (int t = 0; t < 3; ++t) {
            memset(tables[t].bits, 0, sizeof(tables[t].bits));
            fillLut(tables[t], sets[t], counts[t]);
            fillLut(tables[t], HUFF_NEGATIVE, sizeof(HUFF_NEGATIVE) / sizeof(*HUFF_NEGATIVE));
        }
        built = true;
    }
    return tables;
}

// ------------------------------------------------------------------- dither

const int32_t DITHER_LUT[256] = {
     30,  51,  22,  54,   3,   7,  -4,  38,  14,  55,  46,  81,  22,  58,  -3,   2,
     52,  31,  -7,  51,  15,  44,  74,  30,  85, -17,  10,  33,  18,  80,  28,  62,
     10,  32,  23,  69,  72,  26,  35,  17,  73,  60,   8,  56,   2,   6,  -2,  -5,
     51,   4,  11,  50,  66,  76,  21,  44,  33,  47,   1,  26,  64,  48,  57,  40,
     38,  16, -10, -28,  92,  22, -18,  29, -10,   5, -13,  49,  19,  24,  70,  34,
     61,  48,  30,  14,  -6,  25,  58,  33,  42,  60,  67,  17,  54,  17,  22,  30,
     67,  44,  -9,  50, -11,  43,  40,  32,  59,  82,  13,  49, -14,  55,  60,  36,
     48,  49,  31,  47,  15,  12,   4,  65,   1,  23,  29,  39,  45,  -2,  84,  69,
      0,  72,  37,  57,  27,  41, -15, -16,  35,  31,  14,  61,  24,   0,  27,  24,
     16,  41,  55,  34,  53,   9,  56,  12,  25,  29,  53,   5,  20, -20,  -8,  20,
     13,  28,  -3,  78,  38,  16,  11,  62,  46,  29,  21,  24,  46,  65,  43, -23,
     89,  18,  74,  21,  38, -12,  19,  12, -19,   8,  15,  33,   4,  57,   9,  -8,
     36,  35,  26,  28,   7,  83,  63,  79,  75,  11,   3,  87,  37,  47,  34,  40,
     39,  19,  20,  42,  27,  34,  39,  77,  13,  42,  59,  64,  45,  -1,  32,  37,
     45,  -5,  53,  -6,   7,  36,  50,  23,   6,  32,   9, -21,  18,  71,  27,  52,
    -25,  31,  35,  42,  -1,  68,  63,  52,  26,  43,  66,  37,  41,  25,  40,  70,
};

uint32_t mapSamplingFrequency(uint8_t code)
{
    if (code <= 2) {
        return 48000u << code;
    }
    if (code >= 8 && code <= 10) {
        return 44100u << (code - 8);
    }
    throw Error("invalid audio_sampling_frequency code " + std::to_string(code));
}

} // namespace

// ---------------------------------------------------------- substream state
//
// These two are implementation detail, but they cannot live in the anonymous
// namespace above: DecoderState is named by the header and so has external
// linkage, and a type with internal linkage cannot be one of its members.

/// Everything one substream carries between blocks and access units.
struct SubstreamState {
    uint16_t restart_sync_word = 0;
    uint16_t output_timing = 0;

    size_t min_chan = 0;
    size_t max_chan = 0;
    size_t max_matrix_chan = 0;
    uint8_t max_bits = 0;
    int8_t max_shift = 0;
    uint32_t max_lsbs = 0;
    bool error_protect = false;
    uint8_t lossless_check = 0;

    uint32_t dither_shift = 0;
    uint32_t dither_seed = 0;

    size_t ch_assign[MAX_CHANNELS] = {};

    /// One bit per field that a block header may update; all set after a restart.
    uint8_t guards = 0xFF;
    size_t block_size = 8;

    size_t primitive_matrices = 0;
    uint8_t matrix_ch[MAX_CHANNELS] = {};
    uint8_t frac_bits[MAX_CHANNELS] = {};
    int8_t cf_shift_code[MAX_CHANNELS] = {};
    uint8_t dither_scale[MAX_CHANNELS] = {};
    uint8_t delta_bits[MAX_CHANNELS] = {};
    uint8_t delta_precision[MAX_CHANNELS] = {};
    uint16_t cf_mask[MAX_CHANNELS] = {};
    bool lsb_bypass_used[MAX_CHANNELS] = {};
    uint8_t lsb_bypass_bit_count[MAX_CHANNELS] = {};
    int32_t m_coeff[MAX_CHANNELS][MAX_CHANNELS] = {};
    int32_t delta_cf[MAX_CHANNELS][MAX_CHANNELS] = {};

    int8_t output_shift[MAX_CHANNELS] = {};
    uint32_t quantiser_step_size[MAX_CHANNELS] = {};

    int32_t huff_offset[MAX_CHANNELS] = {};
    uint32_t huff_lsbs[MAX_CHANNELS] = {};
    size_t huff_type[MAX_CHANNELS] = {};

    /// [0] is filter A, up to eight taps; [1] is filter B, up to four.
    size_t order[2][MAX_CHANNELS] = {};
    int32_t coeff_q[2][MAX_CHANNELS] = {};
    int32_t coeff[2][MAX_CHANNELS][8] = {};
    int32_t coeff_state[2][MAX_CHANNELS][8] = {};

    int32_t bypassed_lsb[MAX_BLOCK_SIZE][MAX_CHANNELS] = {};
    int32_t block_data[MAX_BLOCK_SIZE][MAX_CHANNELS] = {};
    int32_t rematrix_buffer[MAX_BLOCK_SIZE][MAX_CHANNELS] = {};
    int32_t output_buffer[MAX_BLOCK_SIZE][MAX_CHANNELS] = {};

    int32_t dither_table[256] = {};
    size_t decoded_sample_len = 0;
    size_t zero_samples = 0;

    uint16_t substream_end_ptr = 0;
    bool crc_present = false;
    bool restart_seen = false;

    /// Reset on every restart header: the format expects a decoder joining there
    /// to need nothing carried over. The two directory fields are the exception,
    /// because they describe the segment this header sits inside and were read
    /// before it.
    void resetForRestart()
    {
        SubstreamState fresh;
        fresh.restart_seen = true;
        fresh.substream_end_ptr = substream_end_ptr;
        fresh.crc_present = crc_present;
        *this = fresh;
    }
};

/// Matrix coefficients as read, before the decoder scales them. They are kept
/// apart from the substream state because the state holds the scaled values and
/// a block that carries no new matrix keeps using them.
struct Matrixing {
    bool new_matrix = false;
    bool new_matrix_config = false;
    bool interpolation_used = false;
    bool new_delta = false;
    int32_t m_coeff[MAX_CHANNELS][MAX_CHANNELS] = {};
    int32_t delta_cf[MAX_CHANNELS][MAX_CHANNELS] = {};
};

// --------------------------------------------------------------- decoder

struct DecoderState {
    SubstreamState ss[MAX_SUBSTREAMS];
    int32_t rematrix[MAX_BLOCK_SIZE][MAX_CHANNELS] = {};

    uint32_t format_sync = 0;
    uint32_t sample_rate = 0;
    size_t samples_per_au = 0;
    size_t substreams = 0;
    uint8_t substream_info = 0;
    uint8_t extended_substream_info = 0;
    uint8_t substream_mask = 1;
    size_t presentation = 0;
    size_t channels = 0;
    uint16_t flags = 0;
    bool configured = false;

    void decodeAccessUnit(const uint8_t* data, size_t size, std::vector<int32_t>& out);

private:
    void readMajorSync(BitReader& reader);
    void readSubstreamSegment(BitReader& reader, size_t index, uint64_t segment_start);
    void readBlock(BitReader& reader, size_t index);
    void readRestartHeader(BitReader& reader, size_t index);
    void readBlockHeader(BitReader& reader, size_t index);
    void readMatrixing(BitReader& reader, size_t index);
    void readChannelParams(BitReader& reader, size_t index, size_t chi);
    void readFilterCoeffs(BitReader& reader, size_t index, size_t chi, int type);
    void decodeBlock(size_t index);
    void emit(std::vector<int32_t>& out);
};

void DecoderState::readMajorSync(BitReader& reader)
{
    format_sync = reader.get(32);
    if (format_sync != MAJOR_SYNC_FBA && format_sync != MAJOR_SYNC_FBB) {
        throw Error("unrecognised major sync word");
    }

    // The two syntaxes spend the same 32 bits of format_info differently. Only
    // the sampling frequency matters to a decoder; the rest describes the
    // presentations' channel assignments.
    uint8_t rate_code;
    if (format_sync == MAJOR_SYNC_FBA) {
        rate_code = static_cast<uint8_t>(reader.get(4));
        reader.skip(1 + 1 + 2 + 2 + 2 + 5 + 2 + 13);
    } else {
        reader.skip(8);                 // quantization_word_length_1 and _2
        rate_code = static_cast<uint8_t>(reader.get(4));
        reader.skip(4 + 4 + 4 + 3 + 5);
    }

    sample_rate = mapSamplingFrequency(rate_code);
    // Forty samples per access unit at each family's base rate.
    samples_per_au = (sample_rate / 44100) * 40;
    if (samples_per_au == 0 || samples_per_au > MAX_BLOCK_SIZE) {
        throw Error("access unit sample count out of range");
    }

    uint16_t signature = static_cast<uint16_t>(reader.get(16));
    if (signature != 0xB752) {
        throw Error("major sync signature is not 0xB752");
    }

    flags = static_cast<uint16_t>(reader.get(16));
    reader.skip(16);                    // reserved
    reader.skip(1);                     // variable_rate
    reader.skip(15);                    // peak_data_rate
    substreams = reader.get(4);
    if (substreams == 0 || substreams > MAX_SUBSTREAMS) {
        throw Error("major sync declares " + std::to_string(substreams) + " substreams");
    }
    extended_substream_info = static_cast<uint8_t>(reader.get(4));
    substream_info = static_cast<uint8_t>(reader.get(8));

    // channel_meaning is 64 bits either way; only FBA can extend it.
    if (format_sync == MAJOR_SYNC_FBA) {
        reader.skip(6 + 1 + 1 + 1 + 1 + 7 + 6 + 6 + 5 + 6 + 5 + 5 + 6 + 6 + 1);
        bool extra_present = reader.getBit();
        if (extra_present) {
            uint64_t ecm_start = reader.position();
            uint8_t length = static_cast<uint8_t>(reader.get(4));
            reader.seekTo(ecm_start + ((static_cast<uint64_t>(length) + 1) << 4));
            reader.align16();
        }
    } else {
        reader.skip(64);
    }

    reader.skip(16);                    // major_sync_info_crc

    // Which substreams feed which presentation. The two syntaxes derive this
    // differently and share nothing but the shape of the answer.
    uint8_t masks[MAX_SUBSTREAMS];
    if (format_sync == MAJOR_SYNC_FBB) {
        masks[0] = 1;
        masks[1] = (substream_info & 8) ? 3 : 0;
        masks[2] = 0;
        masks[3] = 0;
    } else {
        masks[0] = 1;
        masks[1] = (substream_info >> 2) & 3;
        masks[2] = (substream_info >> 4) & 7;
        masks[3] = static_cast<uint8_t>(((substream_info >> 4) & 8) |
                                        (7 ^ (7 >> (extended_substream_info & 3))));
    }

    // Decode the highest presentation the stream actually carries: a presentation
    // is its own if its mask reaches its own index, and a copy of a lower one
    // otherwise.
    presentation = 0;
    for (size_t i = 0; i < MAX_SUBSTREAMS; ++i) {
        if (masks[i] >> i) {
            presentation = i;
        }
    }
    substream_mask = masks[presentation];
    configured = true;
}

void DecoderState::readRestartHeader(BitReader& reader, size_t index)
{
    SubstreamState& s = ss[index];
    uint64_t start = reader.position();

    uint16_t sync = static_cast<uint16_t>(reader.get(14));
    if (sync != 0x31EA && sync != 0x31EB && sync != 0x31EC) {
        throw Error("invalid restart sync word " + std::to_string(sync));
    }

    uint16_t output_timing = static_cast<uint16_t>(reader.get(16));
    size_t min_chan = reader.get(4);
    size_t max_chan = reader.get(4);
    size_t max_matrix_chan = reader.get(4);
    uint32_t dither_shift = reader.get(4);
    uint32_t dither_seed = reader.get(23);
    int8_t max_shift = static_cast<int8_t>(reader.get(4));
    uint32_t max_lsbs = reader.get(5);
    uint8_t max_bits = static_cast<uint8_t>(reader.get(5));
    uint8_t max_bits_repeat = static_cast<uint8_t>(reader.get(5));
    bool error_protect = reader.getBit();
    uint8_t lossless_check = static_cast<uint8_t>(reader.get(8));

    if (max_bits != max_bits_repeat) {
        throw Error("restart header max_bits disagrees with its repeat");
    }
    if (max_chan >= MAX_CHANNELS || max_matrix_chan >= MAX_CHANNELS || min_chan > max_chan) {
        throw Error("restart header channel counts out of range");
    }
    // 0x31EA writes two dither channels above the matrix channels.
    if (sync == 0x31EA && max_matrix_chan + 2 >= MAX_CHANNELS) {
        throw Error("restart header leaves no room for the 0x31EA dither channels");
    }

    reader.skip(1);                     // hires_output_timing
    reader.skip(2);

    // The bit and the twelve after it are reserved unless the flags declare
    // heavy DRC, and the bit itself decides whether those twelve are a gain and
    // time update or more reserved space.
    bool heavy_drc_present = false;
    if (flags & 0x2000) {
        heavy_drc_present = reader.getBit();
    } else {
        reader.skip(1);
    }

    if (heavy_drc_present) {
        if (format_sync != MAJOR_SYNC_FBB) {
            reader.skip(9 + 3);         // heavy_drc_gain_update, heavy_drc_time_update
        }
    } else {
        reader.skip(12);
    }

    size_t ch_assign[MAX_CHANNELS] = {};
    uint16_t permutation = 0;
    for (size_t i = 0; i <= max_matrix_chan; ++i) {
        uint8_t assign = static_cast<uint8_t>(reader.get(6));
        if (assign > max_matrix_chan) {
            throw Error("restart header channel assignment out of range");
        }
        uint16_t bit = static_cast<uint16_t>(1u << assign);
        if (permutation & bit) {
            throw Error("restart header repeats a channel assignment");
        }
        permutation |= bit;
        ch_assign[i] = assign;
    }

    uint64_t len = reader.position() - start;
    uint8_t crc_read = static_cast<uint8_t>(reader.get(8));
    uint8_t crc_calc = crc8Range(restartHeaderCrc(), reader.data(), reader.sizeBytes(), start, len);
    if (crc_read != crc_calc) {
        throw Error("restart header CRC mismatch");
    }

    s.resetForRestart();
    s.restart_sync_word = sync;
    s.output_timing = output_timing;
    s.min_chan = min_chan;
    s.max_chan = max_chan;
    s.max_matrix_chan = max_matrix_chan;
    s.dither_shift = dither_shift;
    s.dither_seed = dither_seed;
    s.max_shift = max_shift;
    s.max_lsbs = max_lsbs;
    s.max_bits = max_bits;
    s.error_protect = error_protect;
    s.lossless_check = lossless_check;
    memcpy(s.ch_assign, ch_assign, sizeof(ch_assign));
}

void DecoderState::readFilterCoeffs(BitReader& reader, size_t index, size_t chi, int type)
{
    SubstreamState& s = ss[index];

    size_t order = reader.get(4);
    size_t limit = (type == 0) ? 8 : 4;
    if (order > limit) {
        throw Error("filter order exceeds the limit for its type");
    }

    s.order[type][chi] = order;
    if (order == 0) {
        return;
    }

    uint8_t coeff_q = static_cast<uint8_t>(reader.get(4));
    if (coeff_q < 8) {
        throw Error("filter coefficient shift below 8");
    }
    uint8_t coeff_bits = static_cast<uint8_t>(reader.get(5));
    if (coeff_bits == 0 || coeff_bits > 16) {
        throw Error("filter coefficient width out of range");
    }
    uint8_t coeff_shift = static_cast<uint8_t>(reader.get(3));
    if (coeff_bits + coeff_shift > 16) {
        throw Error("filter coefficient width plus shift exceeds 16");
    }

    s.coeff_q[type][chi] = coeff_q;

    // A new filter replaces all eight taps, so the ones its order does not reach
    // are cleared rather than left holding the previous filter's values.
    int32_t coeffs[8] = {};
    for (size_t i = 0; i < order; ++i) {
        int32_t coeff = reader.getSigned(coeff_bits) << coeff_shift;
        if (coeff == -32768) {
            throw Error("reserved filter coefficient value");
        }
        coeffs[i] = coeff;
    }
    memcpy(s.coeff[type][chi], coeffs, sizeof(coeffs));

    bool new_states = reader.getBit();
    if (new_states) {
        if (type == 0) {
            throw Error("filter A may not carry new states");
        }
        uint8_t state_bits = static_cast<uint8_t>(reader.get(4));
        uint8_t state_shift = static_cast<uint8_t>(reader.get(4));
        int32_t states[8] = {};
        for (size_t i = 0; i < order; ++i) {
            if (state_bits != 0) {
                int32_t state = reader.getSigned(state_bits) << state_shift;
                if (state < -(1 << 23) || state >= (1 << 23)) {
                    throw Error("filter state out of range");
                }
                states[i] = state;
            }
        }
        memcpy(s.coeff_state[type][chi], states, sizeof(states));
    }
}

void DecoderState::readChannelParams(BitReader& reader, size_t index, size_t chi)
{
    SubstreamState& s = ss[index];
    uint8_t guards = s.guards;

    if (guards & (1 << 3)) {            // new_coeffs_a
        if (reader.getBit()) {
            readFilterCoeffs(reader, index, chi, 0);
        }
    }
    if (guards & (1 << 2)) {            // new_coeffs_b
        if (reader.getBit()) {
            readFilterCoeffs(reader, index, chi, 1);
        }
    }

    if (s.order[0][chi] + s.order[1][chi] > 8) {
        throw Error("combined filter order exceeds 8");
    }
    // Filter B alone still needs a shift, and takes the one filter A would use.
    if (s.order[0][chi] == 0 && s.order[1][chi] != 0) {
        s.coeff_q[0][chi] = s.coeff_q[1][chi];
    }

    if (guards & (1 << 1)) {            // new_huff_offset
        if (reader.getBit()) {
            s.huff_offset[chi] = reader.getSigned(15);
        }
    }

    s.huff_type[chi] = reader.get(2);
    s.huff_lsbs[chi] = reader.get(5);

    uint32_t max_huff_lsbs = (s.restart_sync_word == 0x31EC) ? 31 : 24;
    if (s.huff_lsbs[chi] > max_huff_lsbs) {
        throw Error("huff_lsbs exceeds the limit for this substream");
    }
}

void DecoderState::readMatrixing(BitReader& reader, size_t index)
{
    SubstreamState& s = ss[index];
    Matrixing mx;
    size_t max_matrix_chan = s.max_matrix_chan;

    if (s.restart_sync_word == 0x31EC) {
        mx.new_matrix = reader.getBit();

        if (mx.new_matrix) {
            mx.new_matrix_config = reader.getBit();

            if (mx.new_matrix_config) {
                s.primitive_matrices = reader.get(4) + 1;
                for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                    s.matrix_ch[pmi] = static_cast<uint8_t>(reader.get(4));
                    s.frac_bits[pmi] = static_cast<uint8_t>(reader.get(4));
                    s.cf_shift_code[pmi] = static_cast<int8_t>(reader.get(3)) - 1;
                    s.lsb_bypass_bit_count[pmi] = static_cast<uint8_t>(reader.get(2));
                    s.dither_scale[pmi] = static_cast<uint8_t>(reader.get(4));
                    s.cf_mask[pmi] = static_cast<uint16_t>(
                        reader.get(static_cast<unsigned>(max_matrix_chan) + 1));
                }
            }

            for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                unsigned frac_bits = s.frac_bits[pmi];
                uint16_t cf_mask = s.cf_mask[pmi];
                for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
                    mx.m_coeff[pmi][chi] =
                        ((cf_mask >> chi) & 1) ? reader.getSigned(frac_bits + 2) : 0;
                }
            }
        }

        mx.interpolation_used = reader.getBit();

        if (mx.interpolation_used) {
            mx.new_delta = reader.getBit();
            if (mx.new_delta) {
                bool new_delta_config = reader.getBit();
                if (new_delta_config) {
                    for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                        s.delta_bits[pmi] = static_cast<uint8_t>(reader.get(4));
                        s.delta_precision[pmi] = static_cast<uint8_t>(reader.get(2));
                    }
                }
                for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                    uint16_t cf_mask = s.cf_mask[pmi];
                    unsigned delta_bits = s.delta_bits[pmi];
                    for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
                        mx.delta_cf[pmi][chi] =
                            (delta_bits == 0 || ((cf_mask >> chi) & 1) == 0)
                                ? 0 : reader.getSigned(delta_bits + 1);
                    }
                }
            }
        }
    } else {
        s.primitive_matrices = reader.get(4);
        if (s.primitive_matrices > MAX_CHANNELS) {
            throw Error("too many primitive matrices");
        }

        // 0x31EA matrices also take coefficients for the two dither channels.
        size_t coeff_chan = max_matrix_chan + (s.restart_sync_word == 0x31EA ? 2 : 0);
        for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
            s.matrix_ch[pmi] = static_cast<uint8_t>(reader.get(4));
            s.frac_bits[pmi] = static_cast<uint8_t>(reader.get(4));
            s.lsb_bypass_used[pmi] = reader.getBit();

            unsigned coeff_bits = s.frac_bits[pmi] + 2u;
            for (size_t chi = 0; chi <= coeff_chan; ++chi) {
                mx.m_coeff[pmi][chi] = reader.getBit() ? reader.getSigned(coeff_bits) : 0;
            }

            if (s.restart_sync_word == 0x31EB) {
                s.dither_scale[pmi] = static_cast<uint8_t>(reader.get(4));
            }
        }
    }

    for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
        if (s.matrix_ch[pmi] > max_matrix_chan) {
            throw Error("matrix target channel out of range");
        }
        if (s.frac_bits[pmi] > 14) {
            throw Error("matrix frac_bits out of range");
        }
    }

    // Scale the coefficients into the fixed point the recombination expects.
    if (s.restart_sync_word == 0x31EC) {
        if (mx.new_matrix) {
            for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                int shift = 18 + s.cf_shift_code[pmi] - static_cast<int>(s.frac_bits[pmi]);
                for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
                    s.m_coeff[pmi][chi] = mx.m_coeff[pmi][chi] << shift;
                }
            }
        }
        if (mx.interpolation_used) {
            if (mx.new_delta) {
                for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                    int scale = s.cf_shift_code[pmi] - static_cast<int>(s.frac_bits[pmi]) -
                                static_cast<int>(s.delta_precision[pmi]);
                    for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
                        s.delta_cf[pmi][chi] = mx.delta_cf[pmi][chi] << (18 + scale);
                    }
                }
            }
        } else {
            for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
                memset(s.delta_cf[pmi], 0, sizeof(s.delta_cf[pmi]));
            }
        }
    } else {
        size_t coeff_chan = max_matrix_chan + (s.restart_sync_word == 0x31EA ? 2 : 0);
        for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
            for (size_t chi = 0; chi <= coeff_chan; ++chi) {
                s.m_coeff[pmi][chi] = mx.m_coeff[pmi][chi] << (18 - s.frac_bits[pmi]);
            }
        }
    }
}

void DecoderState::readBlockHeader(BitReader& reader, size_t index)
{
    SubstreamState& s = ss[index];

    if (s.guards & (1 << 0)) {          // new_guards
        if (reader.getBit()) {
            s.guards = static_cast<uint8_t>(reader.get(8));
        }
    }

    uint8_t guards = s.guards;

    if (guards & (1 << 7)) {            // new_block_size
        if (reader.getBit()) {
            size_t block_size = reader.get(9);
            if (block_size < 8 || block_size > MAX_BLOCK_SIZE || block_size > samples_per_au) {
                throw Error("block size out of range");
            }
            s.block_size = block_size;
        }
    }

    if (guards & (1 << 6)) {            // new_matrixing
        if (reader.getBit()) {
            readMatrixing(reader, index);
        }
    }

    if (guards & (1 << 5)) {            // new_output_shift
        if (reader.getBit()) {
            for (size_t i = 0; i <= s.max_matrix_chan; ++i) {
                int8_t shift = static_cast<int8_t>(reader.getSigned(4));
                if (shift > s.max_shift) {
                    throw Error("output shift exceeds the stated maximum");
                }
                s.output_shift[i] = shift;
            }
        }
    }

    if (guards & (1 << 4)) {            // new_quantiser_step_size
        if (reader.getBit()) {
            for (size_t i = 0; i <= s.max_chan; ++i) {
                s.quantiser_step_size[i] = reader.get(4);
            }
        }
    }

    for (size_t chi = s.min_chan; chi <= s.max_chan; ++chi) {
        if (reader.getBit()) {          // params_for_this_chan
            readChannelParams(reader, index, chi);
        }
    }
}

void DecoderState::readBlock(BitReader& reader, size_t index)
{
    SubstreamState& s = ss[index];

    if (reader.getBit()) {              // block_header_exists
        if (reader.getBit()) {          // restart_header_exists
            readRestartHeader(reader, index);
        }
        if (!s.restart_seen) {
            throw Error("block header before any restart header");
        }
        readBlockHeader(reader, index);
    }

    if (!s.restart_seen) {
        throw Error("audio block before any restart header");
    }

    uint16_t block_data_bits = 0;
    if (s.error_protect) {
        block_data_bits = static_cast<uint16_t>(reader.get(16));
        if (block_data_bits > 16000) {
            throw Error("block_data_bits too large");
        }
    }

    uint64_t block_data_start = reader.position();

    for (size_t chi = s.min_chan; chi <= s.max_chan; ++chi) {
        if (s.huff_lsbs[chi] > s.max_lsbs) {
            throw Error("huff_lsbs exceeds max_lsbs");
        }
    }

    const HuffLut* huff = huffTables();
    size_t block_size = s.block_size;

    for (size_t blki = 0; blki < block_size; ++blki) {
        // Bits the matrix carries around the lossless recombination rather than
        // through it, so that it stays exactly invertible.
        for (size_t pmi = 0; pmi < s.primitive_matrices; ++pmi) {
            if (s.restart_sync_word == 0x31EC) {
                unsigned count = s.lsb_bypass_bit_count[pmi];
                s.bypassed_lsb[blki][pmi] = count ? static_cast<int32_t>(reader.get(count)) : 0;
            } else {
                s.bypassed_lsb[blki][pmi] =
                    s.lsb_bypass_used[pmi] ? static_cast<int32_t>(reader.get(1)) : 0;
            }
        }

        for (size_t chi = s.min_chan; chi <= s.max_chan; ++chi) {
            uint32_t huff_lsbs = s.huff_lsbs[chi];
            uint32_t qss = s.quantiser_step_size[chi];
            if (qss > huff_lsbs) {
                throw Error("quantiser step size exceeds huff_lsbs");
            }
            unsigned lsbs_bits = huff_lsbs - qss;
            size_t huff_type = s.huff_type[chi];

            int32_t audio_data;
            if (huff_type != 0) {
                // Codes run up to nine bits; near the end of a segment fewer may
                // remain, so the lookup is padded rather than over-read.
                uint64_t avail = reader.available();
                uint64_t here = reader.position();
                uint32_t peek;
                if (avail >= 9) {
                    peek = reader.get(9);
                    reader.seekTo(here);
                } else {
                    peek = 0;
                    for (uint64_t i = 0; i < avail; ++i) {
                        peek = (peek << 1) | (reader.getBit() ? 1u : 0u);
                    }
                    reader.seekTo(here);
                    peek <<= (9 - avail);
                }

                uint8_t bits = huff[huff_type - 1].bits[peek & 0x1FF];
                if (bits == 0 || bits > avail) {
                    throw Error("invalid or truncated Huffman code");
                }
                int32_t huff_code = huff[huff_type - 1].value[peek & 0x1FF];
                reader.skip(bits);

                int32_t lsbs = lsbs_bits ? static_cast<int32_t>(reader.get(lsbs_bits)) : 0;
                int shift = static_cast<int>(lsbs_bits) + (2 - static_cast<int>(huff_type));
                audio_data = lsbs + (huff_code << lsbs_bits) - (shift < 0 ? 0 : (1 << shift));
            } else {
                int32_t lsbs = lsbs_bits ? static_cast<int32_t>(reader.get(lsbs_bits)) : 0;
                audio_data = lsbs - (lsbs_bits ? (1 << (lsbs_bits - 1)) : 0);
            }

            audio_data += s.huff_offset[chi];
            audio_data <<= qss;

            s.block_data[blki][chi] = audio_data;
        }
    }

    if (s.error_protect) {
        uint64_t actual = reader.position() - block_data_start;
        if (actual != block_data_bits) {
            throw Error("block data bit count mismatch");
        }
        reader.skip(8);                 // block_header_crc
    }

    decodeBlock(index);
}

void DecoderState::decodeBlock(size_t index)
{
    SubstreamState& s = ss[index];
    size_t block_size = s.block_size;
    size_t base = s.decoded_sample_len;

    if (base + block_size > MAX_BLOCK_SIZE) {
        throw Error("substream segment carries more samples than an access unit holds");
    }

    // Recorrelation. Two cascaded filters per channel share one shift: filter A
    // predicts from past outputs, filter B from past prediction errors.
    for (size_t chi = s.min_chan; chi <= s.max_chan; ++chi) {
        // The eight saved taps sit above the block so that a single index walks
        // from the newest sample back into the previous block's history.
        int32_t state_buffer[2][MAX_BLOCK_SIZE + 8] = {};
        memcpy(&state_buffer[0][MAX_BLOCK_SIZE], s.coeff_state[0][chi], 8 * sizeof(int32_t));
        memcpy(&state_buffer[1][MAX_BLOCK_SIZE], s.coeff_state[1][chi], 8 * sizeof(int32_t));

        size_t fir_order = s.order[0][chi];
        size_t iir_order = s.order[1][chi];
        int32_t coeff_q_shift = s.coeff_q[0][chi];
        int64_t quantiser_mask = ~((int64_t(1) << s.quantiser_step_size[chi]) - 1);
        const int32_t* fir_coeff = s.coeff[0][chi];
        const int32_t* iir_coeff = s.coeff[1][chi];

        for (size_t blki = 0; blki < block_size; ++blki) {
            int64_t audio_data = s.block_data[blki][chi];
            size_t state_base = MAX_BLOCK_SIZE - blki;

            int64_t acc = 0;
            for (size_t oi = 0; oi < fir_order; ++oi) {
                acc += static_cast<int64_t>(fir_coeff[oi]) * state_buffer[0][state_base + oi];
            }
            for (size_t oi = 0; oi < iir_order; ++oi) {
                acc += static_cast<int64_t>(iir_coeff[oi]) * state_buffer[1][state_base + oi];
            }

            int64_t pred = acc >> coeff_q_shift;
            int64_t fir_state = audio_data + (pred & quantiser_mask);
            int64_t iir_state = fir_state - pred;

            // A conformant stream keeps both inside the substream's sample range,
            // but encoders do run a little past it on near-clipping material and
            // those samples still decode: the arithmetic is 32-bit either way,
            // and rejecting the access unit would drop audio a reference decoder
            // keeps.
            state_buffer[0][MAX_BLOCK_SIZE - 1 - blki] = static_cast<int32_t>(fir_state);
            state_buffer[1][MAX_BLOCK_SIZE - 1 - blki] = static_cast<int32_t>(iir_state);
            s.rematrix_buffer[base + blki][chi] = static_cast<int32_t>(fir_state);
        }

        memcpy(s.coeff_state[0][chi], &state_buffer[0][MAX_BLOCK_SIZE - block_size],
               8 * sizeof(int32_t));
        memcpy(s.coeff_state[1][chi], &state_buffer[1][MAX_BLOCK_SIZE - block_size],
               8 * sizeof(int32_t));
    }

    // Gather every substream feeding this presentation into one buffer.
    for (size_t i = 0; i <= index; ++i) {
        if (((substream_mask >> i) & 1) == 0) {
            continue;
        }
        for (size_t blki = 0; blki < block_size; ++blki) {
            for (size_t ch = ss[i].min_chan; ch <= ss[i].max_chan; ++ch) {
                rematrix[base + blki][ch] = ss[i].rematrix_buffer[base + blki][ch];
            }
        }
    }

    size_t max_matrix_chan = s.max_matrix_chan;
    size_t primitive_matrices = s.primitive_matrices;

    // Lossless matrixing undoes the encoder's inter-channel decorrelation.
    if (s.restart_sync_word == 0x31EA) {
        // Dither is generated inline here, two channels of it per sample, and
        // the matrices take coefficients for both.
        for (size_t blki = 0; blki < block_size; ++blki) {
            int32_t* row = rematrix[base + blki];
            uint32_t seed_shr7 = s.dither_seed >> 7;

            row[max_matrix_chan + 1] =
                static_cast<int32_t>(static_cast<int8_t>(s.dither_seed >> 15)) << s.dither_shift;
            row[max_matrix_chan + 2] =
                static_cast<int32_t>(static_cast<int8_t>(seed_shr7)) << s.dither_shift;

            s.dither_seed = (seed_shr7 ^ (seed_shr7 << 5) ^ (s.dither_seed << 16)) & 0x7FFFFF;

            for (size_t pmi = 0; pmi < primitive_matrices; ++pmi) {
                int64_t acc = 0;
                size_t target = s.matrix_ch[pmi];
                const int32_t* m_coeff = s.m_coeff[pmi];
                for (size_t chi = 0; chi <= max_matrix_chan + 2; ++chi) {
                    acc += static_cast<int64_t>(row[chi]) * m_coeff[chi];
                }
                row[target] = (static_cast<int32_t>(acc >> 18) &
                               ~((1 << s.quantiser_step_size[target]) - 1)) +
                              s.bypassed_lsb[blki][pmi];
            }
        }
    } else if (s.restart_sync_word == 0x31EB || s.restart_sync_word == 0x31EC) {
        // These draw dither from a table built once per access unit instead.
        size_t table_size = 1;
        while (table_size < samples_per_au) {
            table_size <<= 1;
        }

        if (base == 0) {
            for (size_t i = 0; i < table_size; ++i) {
                uint32_t seed_shr15 = s.dither_seed >> 15;
                s.dither_table[i] = DITHER_LUT[seed_shr15 & 0xFF];
                s.dither_seed = ((s.dither_seed << 8) ^ seed_shr15 ^ (seed_shr15 << 5)) & 0x7FFFFF;
            }
        }
        size_t dither_index_mask = table_size - 1;
        int64_t samples_per_au_recip = (int64_t(1) << 16) / static_cast<int64_t>(samples_per_au);
        bool interpolating = (s.restart_sync_word == 0x31EC);

        for (size_t blki = 0; blki < block_size; ++blki) {
            int32_t* row = rematrix[base + blki];
            size_t blki_abs = base + blki;

            for (size_t pmi = 0; pmi < primitive_matrices; ++pmi) {
                int64_t acc = 0;
                int64_t acc_delta = 0;
                size_t target = s.matrix_ch[pmi];
                const int32_t* m_coeff = s.m_coeff[pmi];
                const int32_t* delta_cf = s.delta_cf[pmi];
                int64_t dither_scale = s.dither_scale[pmi];
                size_t dither_index = (primitive_matrices - pmi) * (2 * blki_abs + 1) + blki_abs;

                for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
                    acc += static_cast<int64_t>(row[chi]) * m_coeff[chi];
                    if (interpolating) {
                        acc_delta += static_cast<int64_t>(row[chi]) * delta_cf[chi];
                    }
                }

                if (dither_scale != 0) {
                    acc += static_cast<int64_t>(s.dither_table[dither_index & dither_index_mask])
                           << (11 + dither_scale);
                }

                if (interpolating) {
                    acc += (acc_delta >> 18) * static_cast<int64_t>(blki_abs) *
                           (samples_per_au_recip << 2);
                }

                row[target] = (static_cast<int32_t>(acc >> 18) &
                               ~((1 << s.quantiser_step_size[target]) - 1)) +
                              s.bypassed_lsb[blki][pmi];
            }
        }

        // 0x31EC walks its coefficients across the access unit and commits the
        // step once the last block has used them.
        if (interpolating && base + block_size == samples_per_au) {
            for (size_t pmi = 0; pmi < primitive_matrices; ++pmi) {
                for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
                    s.m_coeff[pmi][chi] += s.delta_cf[pmi][chi];
                }
            }
        }
    }

    // Apply the per-channel output shift and the channel permutation.
    for (size_t blki = 0; blki < block_size; ++blki) {
        const int32_t* row = rematrix[base + blki];
        int32_t* out = s.output_buffer[base + blki];
        memset(out, 0, MAX_CHANNELS * sizeof(int32_t));

        for (size_t chi = 0; chi <= max_matrix_chan; ++chi) {
            int32_t value = row[chi];
            int8_t shift = s.output_shift[chi];
            if (shift < 0) {
                value >>= -shift;
            } else {
                value = static_cast<int32_t>(static_cast<uint32_t>(value) << shift);
            }
            out[s.ch_assign[chi]] = value;
        }
    }

    s.decoded_sample_len += block_size;
}

void DecoderState::readSubstreamSegment(BitReader& reader, size_t index, uint64_t segment_start)
{
    SubstreamState& s = ss[index];
    uint64_t start = reader.position();

    s.decoded_sample_len = 0;
    s.zero_samples = 0;

    bool last_block = false;
    size_t decoded = 0;
    size_t blocks = 0;
    while (!last_block) {
        if (++blocks > MAX_BLOCKS_PER_SEGMENT) {
            throw Error("substream segment carries too many blocks");
        }
        readBlock(reader, index);
        decoded += s.block_size;
        last_block = reader.getBit();
    }

    reader.align16();

    uint64_t expected_end = segment_start + (static_cast<uint64_t>(s.substream_end_ptr) << 4);
    uint64_t here = reader.position();
    if (expected_end < here) {
        throw Error("substream segment overran its end pointer");
    }

    // A termination word may follow, and may say how much of the completed
    // access unit is silence.
    uint64_t test_size = s.crc_present ? 48 : 32;
    if (expected_end - here >= test_size) {
        uint32_t terminator_a = reader.get(18);
        if (terminator_a == 0x348D3) {
            bool zero_samples_indicated = reader.getBit();
            if (zero_samples_indicated) {
                s.zero_samples = reader.get(13);
            } else {
                reader.skip(13);        // terminator_b
            }
        } else {
            reader.seek(-18);
        }
    }

    if (decoded != samples_per_au) {
        throw Error("substream segment decoded " + std::to_string(decoded) +
                    " samples, expected " + std::to_string(samples_per_au));
    }

    if (s.crc_present) {
        uint64_t len = reader.position() - start;
        uint8_t parity = static_cast<uint8_t>(reader.parityByte(len) ^ 0xA9);
        uint8_t parity_read = static_cast<uint8_t>(reader.get(8));
        uint8_t crc_read = static_cast<uint8_t>(reader.get(8));
        uint8_t crc_calc = crc8Range(substreamCrc(), reader.data(), reader.sizeBytes(), start, len);
        if (parity != parity_read) {
            throw Error("substream parity mismatch");
        }
        if (crc_calc != crc_read) {
            throw Error("substream CRC mismatch");
        }
    }

    if (reader.position() != expected_end) {
        throw Error("substream segment did not end where its directory said");
    }
}

void DecoderState::emit(std::vector<int32_t>& out)
{
    SubstreamState& s = ss[presentation];
    channels = s.max_matrix_chan + 1;

    size_t count = samples_per_au;
    if (s.zero_samples < count) {
        count -= s.zero_samples;
    }

    out.reserve(out.size() + count * channels);
    for (size_t i = 0; i < count; ++i) {
        for (size_t ch = 0; ch < channels; ++ch) {
            out.push_back(s.output_buffer[i][ch]);
        }
    }
}

void DecoderState::decodeAccessUnit(const uint8_t* data, size_t size, std::vector<int32_t>& out)
{
    BitReader reader(data, size);

    reader.skip(4);                     // check_nibble
    uint16_t access_unit_length = static_cast<uint16_t>(reader.get(12));
    reader.skip(16);                    // input_timing

    if (static_cast<size_t>(access_unit_length) * 2 > size) {
        throw Error("access unit length exceeds the buffer");
    }

    // The header nibble and the substream directory together parity to 0xF.
    uint8_t parity = reader.parityNibble(32);

    uint64_t here = reader.position();
    uint32_t test = reader.get(32);
    reader.seekTo(here);
    if (test == MAJOR_SYNC_FBA || test == MAJOR_SYNC_FBB) {
        readMajorSync(reader);
    } else if (!configured) {
        throw Error("stream begins without a major sync");
    }

    uint64_t directory_start = reader.position();
    for (size_t i = 0; i < substreams; ++i) {
        SubstreamState& s = ss[i];
        bool extra_substream_word = reader.getBit();
        reader.skip(1);                 // restart_nonexistent
        s.crc_present = reader.getBit();
        reader.skip(1);                 // reserved
        s.substream_end_ptr = static_cast<uint16_t>(reader.get(12));
        if (extra_substream_word) {
            reader.skip(9 + 3 + 4);     // drc_gain_update, drc_time_update, reserved
        }
    }

    if (reader.position() & 7) {
        throw Error("substream directory is not byte aligned");
    }

    parity ^= reader.parityNibble(reader.position() - directory_start);
    if (parity != 0xF) {
        throw Error("access unit header parity mismatch");
    }

    uint64_t segment_start = reader.position();
    for (size_t i = 0; i < substreams; ++i) {
        if (((substream_mask >> i) & 1) == 0) {
            // Not part of the presentation being decoded; step over it.
            uint64_t end = segment_start + (static_cast<uint64_t>(ss[i].substream_end_ptr) << 4);
            if (end < reader.position()) {
                throw Error("skipped substream segment has a backwards end pointer");
            }
            reader.seekTo(end);
            continue;
        }
        readSubstreamSegment(reader, i, segment_start);
    }

    emit(out);
}

// ------------------------------------------------------------------- facade

Decoder::Decoder() : m_state(new DecoderState()) {}
Decoder::~Decoder() = default;

void Decoder::decodeAccessUnit(const uint8_t* data, size_t size, std::vector<int32_t>& out)
{
    m_state->decodeAccessUnit(data, size, out);
}

void Decoder::reset()
{
    m_state.reset(new DecoderState());
}

uint32_t Decoder::sampleRate() const { return m_state->sample_rate; }
size_t Decoder::channels() const { return m_state->channels; }
size_t Decoder::samplesPerAccessUnit() const { return m_state->samples_per_au; }
bool Decoder::configured() const { return m_state->configured; }
bool Decoder::isTrueHD() const { return m_state->format_sync != MAJOR_SYNC_FBB; }

unsigned Decoder::bitsPerSample() const { return 24; }

size_t Decoder::accessUnitLength(const uint8_t* data, size_t size)
{
    if (size < 4) {
        return 0;
    }
    // Twelve bits of length, counted in 16-bit words, below the check nibble.
    return (static_cast<size_t>(data[0] & 0x0F) << 8 | data[1]) * 2;
}

bool Decoder::hasMajorSync(const uint8_t* data, size_t size)
{
    if (size < 8) {
        return false;
    }
    uint32_t sync = static_cast<uint32_t>(data[4]) << 24 | static_cast<uint32_t>(data[5]) << 16 |
                    static_cast<uint32_t>(data[6]) << 8 | static_cast<uint32_t>(data[7]);
    return sync == MAJOR_SYNC_FBA || sync == MAJOR_SYNC_FBB;
}

} // namespace mlp
