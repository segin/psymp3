# PsyMP3 documentation

The design is described in [ARCHITECTURE.md](../ARCHITECTURE.md), with longer
subsystem notes in [ARCHITECTURE_DETAILS.md](ARCHITECTURE_DETAILS.md).
Building is covered in the top-level [README](../README.md), and testing in
[TESTING.md](../TESTING.md).

The rest of this directory is the specifications the demuxers and decoders are
written against. They are kept here so that the section numbers cited in the
source can be looked up without a network connection.

## Specifications

| Format | Document |
|---|---|
| FLAC | [rfc9639.txt](rfc9639.txt) |
| Ogg | [rfc3533.txt](rfc3533.txt) — the bitstream; [rfc3534.txt](rfc3534.txt) — its media type |
| Opus | [rfc6716.txt](rfc6716.txt) — the codec; [rfc7845.txt](rfc7845.txt) — Opus in Ogg |
| Vorbis | [vorbis-spec.html](vorbis-spec.html) — Vorbis I, including Vorbis in Ogg |
| Speex | [speex-manual.pdf](speex-manual.pdf); [rfc5574.txt](rfc5574.txt) — RTP payload and media types |
| Matroska, WebM | [rfc8794.txt](rfc8794.txt) — EBML; [rfc9559.txt](rfc9559.txt) — Matroska; [webm-container-guidelines.txt](webm-container-guidelines.txt) — the WebM subset |
| MP4, M4A | ISO/IEC 14496-12:2015, the ISO base media file format ([PDF](ISO_IEC_14496-12_2015.pdf), [text](ISO_IEC_14496-12_2015.txt)); [rfc4337.txt](rfc4337.txt) — MPEG-4 media types |
| AC-3, E-AC-3 | [atsc-a52.txt](atsc-a52.txt) — where to download ATSC A/52, which is not kept here |
