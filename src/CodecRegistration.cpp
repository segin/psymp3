/*
 * CodecRegistration.cpp - Centralized codec and demuxer registration functions
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

void registerAllCodecs() {
    Debug::log("codec", "registerAllCodecs: Starting codec registration");
    
    // Register PCM codec (always available)
    CodecRegistry::registerCodec("pcm", [](const StreamInfo& info) {
        return std::make_unique<PCMCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered PCM codec");
    
#ifdef ENABLE_ALAW_CODEC
    registerALawCodec();
    Debug::log("codec", "registerAllCodecs: Registered A-law codec");
#else
    Debug::log("codec", "registerAllCodecs: A-law codec disabled at compile time");
#endif
    
#ifdef ENABLE_MULAW_CODEC
    registerMuLawCodec();
    Debug::log("codec", "registerAllCodecs: Registered μ-law codec");
#else
    Debug::log("codec", "registerAllCodecs: μ-law codec disabled at compile time");
#endif

    // MP3 remains in legacy Stream architecture - not registered with CodecRegistry
#ifdef HAVE_MP3
    Debug::log("codec", "registerAllCodecs: MP3 codec uses legacy Stream architecture (not registered)");
#else
    Debug::log("codec", "registerAllCodecs: MP3 codec disabled at compile time");
#endif

#ifdef HAVE_VORBIS
    CodecRegistry::registerCodec("vorbis", [](const StreamInfo& info) {
        return std::make_unique<VorbisCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered Vorbis codec");
    
    // Also register the passthrough variant for Ogg containers
    CodecRegistry::registerCodec("vorbis_passthrough", [](const StreamInfo& info) {
        return std::make_unique<VorbisPassthroughCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered Vorbis passthrough codec");
#else
    Debug::log("codec", "registerAllCodecs: Vorbis codec disabled at compile time");
#endif

#ifdef HAVE_OPUS
    CodecRegistry::registerCodec("opus", [](const StreamInfo& info) {
        return std::make_unique<OpusCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered Opus codec");
    
    // Also register the passthrough variant for Ogg containers
    CodecRegistry::registerCodec("opus_passthrough", [](const StreamInfo& info) {
        return std::make_unique<OpusPassthroughCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered Opus passthrough codec");
#else
    Debug::log("codec", "registerAllCodecs: Opus codec disabled at compile time");
#endif

    // FLAC remains in legacy Stream architecture - not registered with CodecRegistry
#ifdef HAVE_FLAC
    Debug::log("codec", "registerAllCodecs: FLAC codec uses legacy Stream architecture (not registered)");
    
    // Register Ogg FLAC passthrough codec if Ogg support is available
#ifdef HAVE_OGGDEMUXER
    CodecRegistry::registerCodec("ogg_flac_passthrough", [](const StreamInfo& info) {
        return std::make_unique<OggFLACPassthroughCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered Ogg FLAC passthrough codec");
#endif
#else
    Debug::log("codec", "registerAllCodecs: FLAC codec disabled at compile time");
#endif

    // Register Speex codec if Ogg support is available (Speex is always in Ogg containers)
#ifdef HAVE_OGGDEMUXER
    CodecRegistry::registerCodec("speex", [](const StreamInfo& info) {
        return std::make_unique<SpeexCodec>(info);
    });
    Debug::log("codec", "registerAllCodecs: Registered Speex codec");
#endif

    Debug::log("codec", "registerAllCodecs: Codec registration completed, total codecs: ", 
               CodecRegistry::getRegisteredCodecCount());
}

void registerAllDemuxers() {
    Debug::log("demuxer", "registerAllDemuxers: Starting demuxer registration");
    
    // Always register these demuxers (no conditional compilation)
    DemuxerRegistry::getInstance().registerDemuxer("riff", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<ChunkDemuxer>(std::move(handler));
    }, "RIFF/WAVE", {"wav", "wave"});
    Debug::log("demuxer", "registerAllDemuxers: Registered RIFF demuxer");
    
    DemuxerRegistry::getInstance().registerDemuxer("aiff", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<ChunkDemuxer>(std::move(handler));
    }, "AIFF", {"aiff", "aif"});
    Debug::log("demuxer", "registerAllDemuxers: Registered AIFF demuxer");
    
    DemuxerRegistry::getInstance().registerDemuxer("mp4", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<ISODemuxer>(std::move(handler));
    }, "MP4/ISO", {"mp4", "m4a", "mov"});
    Debug::log("demuxer", "registerAllDemuxers: Registered MP4/ISO demuxer");
    
    DemuxerRegistry::getInstance().registerDemuxer("raw_audio", [](std::unique_ptr<IOHandler> handler) {
        // Note: RawAudioDemuxer needs file path for format detection
        // This factory will need to be enhanced when MediaFactory is updated
        return std::make_unique<RawAudioDemuxer>(std::move(handler), "");
    }, "Raw Audio", {"pcm", "raw"});
    Debug::log("demuxer", "registerAllDemuxers: Registered raw audio demuxer");

    // FLAC uses legacy Stream architecture - no separate demuxer needed
#ifdef HAVE_FLAC
    Debug::log("demuxer", "registerAllDemuxers: FLAC uses legacy Stream architecture (no demuxer registration needed)");
#else
    Debug::log("demuxer", "registerAllDemuxers: FLAC disabled at compile time");
#endif

    // OggDemuxer registration depends on available Ogg codecs
    // Register if any of: Vorbis, Opus, or (FLAC + Ogg) are available
#if defined(HAVE_VORBIS) || defined(HAVE_OPUS) || (defined(HAVE_FLAC) && defined(HAVE_OGG))
    DemuxerRegistry::getInstance().registerDemuxer("ogg", [](std::unique_ptr<IOHandler> handler) {
        return std::make_unique<OggDemuxer>(std::move(handler));
    }, "Ogg", {"ogg", "oga", "ogv", "ogx"});
    
    std::string ogg_codecs = "OggDemuxer registered with support for: ";
#ifdef HAVE_VORBIS
    ogg_codecs += "Vorbis ";
#endif
#ifdef HAVE_OPUS
    ogg_codecs += "Opus ";
#endif
#if defined(HAVE_FLAC) && defined(HAVE_OGG)
    ogg_codecs += "FLAC ";
#endif
    Debug::log("demuxer", "registerAllDemuxers: ", ogg_codecs);
#else
    Debug::log("demuxer", "registerAllDemuxers: OggDemuxer disabled - no Ogg-compatible codecs available");
#endif

    Debug::log("demuxer", "registerAllDemuxers: Demuxer registration completed, total demuxers: ", 
               DemuxerRegistry::getInstance().getSupportedFormats().size());
}