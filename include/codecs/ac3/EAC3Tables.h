/*
 * EAC3Tables.h - E-AC-3 tables from A/52 Annex E
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_EAC3TABLES_H
#define PSYMP3_CODECS_AC3_EAC3TABLES_H

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// A/52 Table E2.10, frame exponent strategy combinations.
///
/// When audfrm() sends expstre = 0 the per-block exponent strategies are not
/// transmitted: each channel gets one 5-bit frmchexpstr (and the coupling
/// channel one frmcplexpstr), and that code selects a row here giving the
/// strategy for each of the six blocks. Only six-block frames can use it --
/// expstre is implicitly 1 otherwise.
///
/// Extracted mechanically from the printed table and held to the properties
/// a transcription slip would break: 32 rows of six, entries only D15, D25,
/// D45 or reuse, block 0 never reuse (a frame must open with exponents), and
/// no two rows alike.
constexpr ExponentStrategy kFrameExponentStrategies[32][6] = {
    { ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  //  0
    { ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45 },  //  1
    { ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse },  //  2
    { ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45 },  //  3
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  //  4
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45 },  //  5
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse },  //  6
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45 },  //  7
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  //  8
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45 },  //  9
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse },  // 10
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45 },  // 11
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  // 12
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45 },  // 13
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse },  // 14
    { ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45 },  // 15
    { ExponentStrategy::D45, ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  // 16
    { ExponentStrategy::D45, ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45 },  // 17
    { ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse },  // 18
    { ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45 },  // 19
    { ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  // 20
    { ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45 },  // 21
    { ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse },  // 22
    { ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45 },  // 23
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D15, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  // 24
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse, ExponentStrategy::D45 },  // 25
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D25, ExponentStrategy::Reuse },  // 26
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45, ExponentStrategy::D45 },  // 27
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::Reuse },  // 28
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse, ExponentStrategy::D45 },  // 29
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D25, ExponentStrategy::Reuse },  // 30
    { ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45, ExponentStrategy::D45 },  // 31
};

/// A/52 Table E2.12, defcplbndstrc[], by absolute coupling sub-band number.
///
/// An E-AC-3 block may omit the coupling band structure (cplbndstrce = 0).
/// In the first coupled block of a frame that means this default; in any
/// later block it means the previous block's structure. Entry 0 is not in
/// the table -- sub-band 0 always opens a band -- and is 0 here to keep the
/// index equal to the sub-band number. A set entry merges that sub-band into
/// the band before it, so the default leaves the low sub-bands separate and
/// groups the high ones, where the ear resolves less.
constexpr uint8_t kDefaultCouplingBandStructure[18] = {
    0,                                  // sub-band 0, not in the table
    0, 0, 0, 0, 0, 0, 0, 1, 0,          // 1..9
    1, 1, 0, 1, 1, 1, 1, 1,             // 10..17
};

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_EAC3TABLES_H
