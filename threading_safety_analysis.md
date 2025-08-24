# Threading Safety Analysis Report
==================================================

Found 30 classes with threading concerns:

## Class: Audio
File: include/audio.h

### Mutex Members:
  - m_buffer_mutex
  - m_stream_mutex

### ⚠️  Potential Issues:
  - Class has multiple mutexes (m_buffer_mutex, m_stream_mutex) - verify lock acquisition order

----------------------------------------

## Class: BoundedQueue
File: include/BoundedQueue.h

### Mutex Members:
  - m_mutex

### Public Lock-Acquiring Methods:
  - m_current_memory_bytes (line 15)
  - if (line 18)
  - sizeof (line 19)
  - tryPush (line 33)
  - lock (line 34)
  - m_memory_calculator (line 36)
  - tryPush (line 59)
  - tryPop (line 69)
  - lock (line 70)
  - pop (line 77)
  - m_memory_calculator (line 80)
  - size (line 91)
  - lock (line 92)
  - size (line 93)
  - empty (line 100)
  - lock (line 101)
  - empty (line 102)
  - clear (line 108)
  - lock (line 109)
  - pop (line 111)
  - memoryUsage (line 120)
  - lock (line 121)
  - setMaxItems (line 129)
  - lock (line 130)
  - setMaxMemoryBytes (line 138)
  - lock (line 139)
  - getMaxItems (line 147)
  - lock (line 148)
  - getMaxMemoryBytes (line 156)
  - lock (line 157)

### ⚠️  Potential Issues:
  - Public method 'm_current_memory_bytes' acquires locks but has no private 'm_current_memory_bytes_unlocked' counterpart
  - Public method 'if' acquires locks but has no private 'if_unlocked' counterpart
  - Public method 'sizeof' acquires locks but has no private 'sizeof_unlocked' counterpart
  - Public method 'tryPush' acquires locks but has no private 'tryPush_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'm_memory_calculator' acquires locks but has no private 'm_memory_calculator_unlocked' counterpart
  - Public method 'tryPush' acquires locks but has no private 'tryPush_unlocked' counterpart
  - Public method 'tryPop' acquires locks but has no private 'tryPop_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'pop' acquires locks but has no private 'pop_unlocked' counterpart
  - Public method 'm_memory_calculator' acquires locks but has no private 'm_memory_calculator_unlocked' counterpart
  - Public method 'size' acquires locks but has no private 'size_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'size' acquires locks but has no private 'size_unlocked' counterpart
  - Public method 'empty' acquires locks but has no private 'empty_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'empty' acquires locks but has no private 'empty_unlocked' counterpart
  - Public method 'clear' acquires locks but has no private 'clear_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'pop' acquires locks but has no private 'pop_unlocked' counterpart
  - Public method 'memoryUsage' acquires locks but has no private 'memoryUsage_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'setMaxItems' acquires locks but has no private 'setMaxItems_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'setMaxMemoryBytes' acquires locks but has no private 'setMaxMemoryBytes_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'getMaxItems' acquires locks but has no private 'getMaxItems_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'getMaxMemoryBytes' acquires locks but has no private 'getMaxMemoryBytes_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart

----------------------------------------

## Class: BufferPool
File: include/Demuxer.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: CurlLifecycleManager
File: src/HTTPClient.cpp

### Mutex Members:
  - s_cleanup_mutex
  - s_pool_mutex

### Public Lock-Acquiring Methods:
  - call_once (line 20)
  - curl_global_init (line 21)
  - if (line 23)
  - log (line 24)
  - log (line 26)
  - isInitialized (line 35)
  - incrementHandleCount (line 37)
  - fetch_add (line 38)
  - decrementHandleCount (line 41)
  - fetch_sub (line 42)
  - if (line 43)
  - cleanup_thread (line 45)
  - performCleanupIfNeeded (line 47)
  - detach (line 49)
  - forceCleanup (line 53)
  - call_once (line 54)
  - lock (line 55)
  - if (line 56)
  - cleanupConnectionPool (line 58)
  - curl_global_cleanup (line 59)
  - log (line 61)
  - acquireConnection (line 67)
  - lock (line 68)
  - now (line 71)
  - if (line 72)
  - cleanupExpiredConnections (line 73)
  - find (line 77)
  - back (line 79)
  - pop_back (line 80)
  - log (line 81)
  - curl_easy_init (line 86)
  - if (line 87)
  - log (line 88)
  - releaseConnection (line 93)
  - lock (line 96)
  - curl_easy_reset (line 101)
  - push_back (line 102)
  - log (line 103)
  - curl_easy_cleanup (line 106)
  - log (line 107)
  - cleanupConnectionPool (line 111)
  - lock (line 112)
  - for (line 114)
  - for (line 115)
  - curl_easy_cleanup (line 116)
  - clear (line 118)
  - clear (line 120)
  - log (line 121)

### ⚠️  Potential Issues:
  - Public method 'call_once' acquires locks but has no private 'call_once_unlocked' counterpart
  - Public method 'curl_global_init' acquires locks but has no private 'curl_global_init_unlocked' counterpart
  - Public method 'if' acquires locks but has no private 'if_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'isInitialized' acquires locks but has no private 'isInitialized_unlocked' counterpart
  - Public method 'incrementHandleCount' acquires locks but has no private 'incrementHandleCount_unlocked' counterpart
  - Public method 'fetch_add' acquires locks but has no private 'fetch_add_unlocked' counterpart
  - Public method 'decrementHandleCount' acquires locks but has no private 'decrementHandleCount_unlocked' counterpart
  - Public method 'fetch_sub' acquires locks but has no private 'fetch_sub_unlocked' counterpart
  - Public method 'if' acquires locks but has no private 'if_unlocked' counterpart
  - Public method 'cleanup_thread' acquires locks but has no private 'cleanup_thread_unlocked' counterpart
  - Public method 'performCleanupIfNeeded' acquires locks but has no private 'performCleanupIfNeeded_unlocked' counterpart
  - Public method 'detach' acquires locks but has no private 'detach_unlocked' counterpart
  - Public method 'forceCleanup' acquires locks but has no private 'forceCleanup_unlocked' counterpart
  - Public method 'call_once' acquires locks but has no private 'call_once_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'if' acquires locks but has no private 'if_unlocked' counterpart
  - Public method 'cleanupConnectionPool' acquires locks but has no private 'cleanupConnectionPool_unlocked' counterpart
  - Public method 'curl_global_cleanup' acquires locks but has no private 'curl_global_cleanup_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'acquireConnection' acquires locks but has no private 'acquireConnection_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'now' acquires locks but has no private 'now_unlocked' counterpart
  - Public method 'if' acquires locks but has no private 'if_unlocked' counterpart
  - Public method 'cleanupExpiredConnections' acquires locks but has no private 'cleanupExpiredConnections_unlocked' counterpart
  - Public method 'find' acquires locks but has no private 'find_unlocked' counterpart
  - Public method 'back' acquires locks but has no private 'back_unlocked' counterpart
  - Public method 'pop_back' acquires locks but has no private 'pop_back_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'curl_easy_init' acquires locks but has no private 'curl_easy_init_unlocked' counterpart
  - Public method 'if' acquires locks but has no private 'if_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'releaseConnection' acquires locks but has no private 'releaseConnection_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'curl_easy_reset' acquires locks but has no private 'curl_easy_reset_unlocked' counterpart
  - Public method 'push_back' acquires locks but has no private 'push_back_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'curl_easy_cleanup' acquires locks but has no private 'curl_easy_cleanup_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Public method 'cleanupConnectionPool' acquires locks but has no private 'cleanupConnectionPool_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'for' acquires locks but has no private 'for_unlocked' counterpart
  - Public method 'for' acquires locks but has no private 'for_unlocked' counterpart
  - Public method 'curl_easy_cleanup' acquires locks but has no private 'curl_easy_cleanup_unlocked' counterpart
  - Public method 'clear' acquires locks but has no private 'clear_unlocked' counterpart
  - Public method 'clear' acquires locks but has no private 'clear_unlocked' counterpart
  - Public method 'log' acquires locks but has no private 'log_unlocked' counterpart
  - Class has multiple mutexes (s_cleanup_mutex, s_pool_mutex) - verify lock acquisition order

----------------------------------------

## Class: Debug
File: include/debug.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: Demuxer
File: include/Demuxer.h

### Mutex Members:
  - m_error_mutex
  - m_state_mutex
  - m_streams_mutex
  - m_io_mutex

### Public Lock-Acquiring Methods:
  - getGranulePosition (line 129)
  - getLastError (line 136)
  - lock (line 137)
  - hasError (line 144)
  - lock (line 145)
  - empty (line 146)
  - clearError (line 152)
  - lock (line 153)
  - isParsed (line 160)
  - lock (line 161)
  - isEOFAtomic (line 168)
  - load (line 169)
  - setEOF (line 175)
  - store (line 176)

### ⚠️  Potential Issues:
  - Public method 'getGranulePosition' acquires locks but has no private 'getGranulePosition_unlocked' counterpart
  - Public method 'getLastError' acquires locks but has no private 'getLastError_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'hasError' acquires locks but has no private 'hasError_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'empty' acquires locks but has no private 'empty_unlocked' counterpart
  - Public method 'clearError' acquires locks but has no private 'clearError_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'isParsed' acquires locks but has no private 'isParsed_unlocked' counterpart
  - Public method 'lock' acquires locks but has no private 'lock_unlocked' counterpart
  - Public method 'isEOFAtomic' acquires locks but has no private 'isEOFAtomic_unlocked' counterpart
  - Public method 'load' acquires locks but has no private 'load_unlocked' counterpart
  - Public method 'setEOF' acquires locks but has no private 'setEOF_unlocked' counterpart
  - Public method 'store' acquires locks but has no private 'store_unlocked' counterpart
  - Class has multiple mutexes (m_error_mutex, m_state_mutex, m_streams_mutex, m_io_mutex) - verify lock acquisition order

----------------------------------------

## Class: DemuxerConfigManager
File: include/DemuxerExtensibility.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: DemuxerExtensionPoint
File: include/DemuxerExtensibility.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: DemuxerFactory
File: include/DemuxerFactory.h

### Mutex Members:
  - s_factory_mutex

----------------------------------------

## Class: DemuxerPluginManager
File: include/DemuxerPlugin.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: DemuxerRegistry
File: include/DemuxerRegistry.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: EnhancedAudioBufferPool
File: include/EnhancedAudioBufferPool.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: EnhancedBufferPool
File: include/EnhancedBufferPool.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: FileIOHandler
File: include/FileIOHandler.h

### Mutex Members:
  - m_file_mutex
  - m_operation_mutex
  - m_buffer_mutex

### ⚠️  Potential Issues:
  - Class has multiple mutexes (m_file_mutex, m_operation_mutex, m_buffer_mutex) - verify lock acquisition order

----------------------------------------

## Class: FlacDecoder
File: include/flac.h

### Mutex Members:
  - m_output_buffer_mutex

----------------------------------------

## Class: HTTPIOHandler
File: include/HTTPIOHandler.h

### Mutex Members:
  - m_http_mutex
  - m_initialization_mutex
  - m_buffer_mutex

### ⚠️  Potential Issues:
  - Class has multiple mutexes (m_http_mutex, m_initialization_mutex, m_buffer_mutex) - verify lock acquisition order

----------------------------------------

## Class: IOBufferPool
File: include/BufferPool.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: IOHandler
File: include/IOHandler.h

### Mutex Members:
  - m_state_mutex
  - s_memory_mutex
  - m_operation_mutex

### ⚠️  Potential Issues:
  - Class has multiple mutexes (m_state_mutex, s_memory_mutex, m_operation_mutex) - verify lock acquisition order

----------------------------------------

## Class: IOHandlerRegistry
File: include/DemuxerExtensibility.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: LastFM
File: include/LastFM.h

### Mutex Members:
  - m_scrobble_mutex

----------------------------------------

## Class: MediaFactory
File: include/MediaFactory.h

### Mutex Members:
  - s_factory_mutex

----------------------------------------

## Class: MemoryOptimizer
File: include/MemoryOptimizer.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: MemoryPoolManager
File: include/MemoryPoolManager.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: MemoryTracker
File: include/MemoryTracker.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: MetadataExtensionRegistry
File: include/DemuxerExtensibility.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: OggDemuxer
File: include/OggDemuxer.h

### Mutex Members:
  - m_ogg_state_mutex
  - m_packet_queue_mutex

### ⚠️  Potential Issues:
  - Class has multiple mutexes (m_ogg_state_mutex, m_packet_queue_mutex) - verify lock acquisition order

----------------------------------------

## Class: Player
File: include/player.h

### Mutex Members:
  - m_loader_queue_mutex

----------------------------------------

## Class: Playlist
File: include/playlist.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: StreamFactoryRegistry
File: include/DemuxerExtensibility.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Class: StreamingManager
File: include/StreamingManager.h

### Mutex Members:
  - m_mutex

----------------------------------------

## Summary
- Classes analyzed: 30
- Public lock-acquiring methods: 92
- Potential threading issues: 99