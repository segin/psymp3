// PsyMP3 Final Build - Single Compilation Unit
// This file includes all source files for optimized compilation
// Similar to KDE3's finalized builds
//
// Enable final build with: ./configure --enable-final
//
// Note: This must be kept in sync with Makefile.am source lists when files are
// added/removed

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// ============================================================================
// Core Application
// ============================================================================
#include "audio.cpp"
#include "core/about.cpp"
#include "core/fft.cpp"
#include "core/fft_draw.cpp"
#include "core/persistentstorage.cpp"
#include "core/rect.cpp"
#include "core/song.cpp"
#include "debug.cpp"
#include "display.cpp"
#include "exceptions.cpp"
#include "font.cpp"
#include "core/lyrics.cpp"
#include "main.cpp"
#include "mediafile.cpp"
#include "player.cpp"
#include "playlist.cpp"
#include "stream.cpp"
#include "surface.cpp"
#include "system.cpp"
#include "track.cpp"
#include "truetype.cpp"

// ============================================================================
// Utility
// ============================================================================
#include "core/utility/XMLUtil.cpp"
#include "core/utility/utility.cpp"

// ============================================================================
// I/O Subsystem
// ============================================================================
#include "io/BoundedBuffer.cpp"
#include "io/BufferPool.cpp"
#include "io/EnhancedAudioBufferPool.cpp"
#include "io/EnhancedBufferPool.cpp"
#include "io/IOHandler.cpp"
#include "io/MemoryOptimizer.cpp"
#include "io/MemoryPoolManager.cpp"
#include "io/MemoryTracker.cpp"
#include "io/RAIIFileHandle.cpp"
#include "io/StreamingManager.cpp"
#include "io/TagLibIOHandlerAdapter.cpp"
#include "io/URI.cpp"
#include "io/file/FileIOHandler.cpp"
#include "io/http/HTTPClient.cpp"
#include "io/http/HTTPIOHandler.cpp"

// ============================================================================
// Demuxer Subsystem - Core
// ============================================================================
#include "demuxer/ChainedStream.cpp"
#include "demuxer/ChunkDemuxer.cpp"
#include "demuxer/DemuxedStream.cpp"
#include "demuxer/Demuxer.cpp"
#include "demuxer/DemuxerExtensibility.cpp"
#include "demuxer/DemuxerFactory.cpp"
#include "demuxer/DemuxerPlugin.cpp"
#include "demuxer/DemuxerRegistry.cpp"
#include "demuxer/MediaFactory.cpp"
#include "demuxer/ModernStream.cpp"

// Demuxer - Raw Audio
#include "demuxer/raw/RawAudioDemuxer.cpp"

// Demuxer - ISO/MP4
#include "demuxer/iso/BoxParser.cpp"
#include "demuxer/iso/ComplianceValidator.cpp"
#include "demuxer/iso/ErrorRecovery.cpp"
#include "demuxer/iso/FragmentHandler.cpp"
#include "demuxer/iso/ISODemuxer.cpp"
#include "demuxer/iso/MetadataExtractor.cpp"
#include "demuxer/iso/SampleTableManager.cpp"
#include "demuxer/iso/SeekingEngine.cpp"
#include "demuxer/iso/StreamManager.cpp"

// Demuxer - RIFF/WAV
#include "demuxer/riff/wav.cpp"

// ============================================================================
// Codec Subsystem - Core
// ============================================================================
#include "codecs/AudioCodec.cpp"
#include "codecs/CodecRegistration.cpp"
#include "codecs/CodecRegistry.cpp"

// Codec - PCM (always built)
#include "codecs/pcm/PCMCodecs.cpp"

// Codec - G.711 (optional)
#ifdef ENABLE_ALAW_CODEC
#include "codecs/pcm/ALawCodec.cpp"
#endif

#ifdef ENABLE_MULAW_CODEC
#include "codecs/pcm/MuLawCodec.cpp"
#endif

// ============================================================================
// Optional Codec/Demuxer: Ogg container (Vorbis, Opus, FLAC)
// ============================================================================
#ifdef HAVE_OGGDEMUXER
#include "codecs/OggCodecs.cpp"
#include "demuxer/ogg/CodecHeaderParser.cpp"
#include "demuxer/ogg/FLACHeaderParser.cpp"
#include "demuxer/ogg/OggDemuxer.cpp"
#include "demuxer/ogg/OggSeekingEngine.cpp"
#include "demuxer/ogg/OggStreamManager.cpp"
#include "demuxer/ogg/OggSyncManager.cpp"
#include "demuxer/ogg/OpusHeaderParser.cpp"
#include "demuxer/ogg/SpeexHeaderParser.cpp"
#include "demuxer/ogg/VorbisHeaderParser.cpp"
#endif

// ============================================================================
// Optional Codec: Vorbis
// ============================================================================
#ifdef HAVE_VORBIS
#include "codecs/vorbis/VorbisCodec.cpp"
#endif

// ============================================================================
// Optional Codec: Opus
// ============================================================================
#ifdef HAVE_OPUS
#include "codecs/opus/OpusCodec.cpp"
#include "codecs/opus/opus.cpp"
#endif

// ============================================================================
// Optional Codec/Demuxer: FLAC
// ============================================================================
#ifdef HAVE_FLAC
// FLAC Demuxer (always needed for FLAC container)
#include "demuxer/flac/FLACDemuxer.cpp"

// FLAC Codec support files
#include "codecs/flac/FLACPerformanceBenchmark.cpp"
#include "codecs/flac/FLACRFC9639.cpp"
#include "codecs/flac/FLACRFCValidator.cpp"

#ifdef HAVE_NATIVE_FLAC
// Native FLAC decoder (no libFLAC dependency)
#include "codecs/flac/BitstreamReader.cpp"
#include "codecs/flac/CRCValidator.cpp"
#include "codecs/flac/ChannelDecorrelator.cpp"
#include "codecs/flac/FrameParser.cpp"
#include "codecs/flac/MD5Validator.cpp"
#include "codecs/flac/MetadataParser.cpp"
#include "codecs/flac/NativeFLACCodec.cpp"
#include "codecs/flac/ResidualDecoder.cpp"
#include "codecs/flac/SampleReconstructor.cpp"
#include "codecs/flac/SubframeDecoder.cpp"
#else
// libFLAC wrapper
#include "codecs/flac/FLACCodec.cpp"
#include "codecs/flac/LibFLACWrapper.cpp"
#endif // HAVE_NATIVE_FLAC
#endif // HAVE_FLAC

// ============================================================================
// Optional Codec: MP3
// ============================================================================
#ifdef HAVE_MP3
#include "codecs/mp3/MP3Codec.cpp"
#endif

// ============================================================================
// Widget System - Foundation
// ============================================================================
#include "widget/foundation/DrawableWidget.cpp"
#include "widget/foundation/FadingWidget.cpp"
#include "widget/foundation/LayoutWidget.cpp"
#include "widget/foundation/Widget.cpp"

// ============================================================================
// Widget System - Windowing
// ============================================================================
#include "widget/windowing/TitlebarWidget.cpp"
#include "widget/windowing/TransparentWindowWidget.cpp"
#include "widget/windowing/WindowFrameWidget.cpp"
#include "widget/windowing/WindowWidget.cpp"

// ============================================================================
// Widget System - UI
// ============================================================================
#include "widget/ui/ApplicationWidget.cpp"
#include "widget/ui/ButtonWidget.cpp"
#include "widget/ui/Label.cpp"
#include "widget/ui/LyricsWidget.cpp"
#include "widget/ui/MainUIWidget.cpp"
#include "widget/ui/PlayerProgressBarWidget.cpp"
#include "widget/ui/ProgressBarBracketWidget.cpp"
#include "widget/ui/ProgressBarFrameWidget.cpp"
#include "widget/ui/SpectrumAnalyzerWidget.cpp"
#include "widget/ui/ToastNotification.cpp"
#include "widget/ui/ToastWidget.cpp"

// ============================================================================
// Tag Subsystem
// ============================================================================
#include "tag/Tag.cpp"
#include "tag/NullTag.cpp"

// ============================================================================
// Last.fm Scrobbling
// ============================================================================
#include "lastfm/LastFM.cpp"
#include "lastfm/scrobble.cpp"

// ============================================================================
// MPRIS D-Bus Integration (conditional)
// ============================================================================
#ifdef HAVE_DBUS
#include "mpris/DBusConnectionManager.cpp"
#include "mpris/MPRISManager.cpp"
#include "mpris/MPRISTypes.cpp"
#include "mpris/MethodHandler.cpp"
#include "mpris/PropertyManager.cpp"
#include "mpris/SignalEmitter.cpp"
#endif