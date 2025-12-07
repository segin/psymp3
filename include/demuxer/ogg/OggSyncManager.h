/*
 * OggSyncManager.h - Ogg Sync Layer (RFC 3533)
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * This class handles the lowest layer of Ogg decoding:
 * - Reads data from IOHandler
 * - Synchronizes with Ogg capture pattern "OggS"
 * - Extracts complete Ogg pages
 *
 * It wraps libogg's ogg_sync_state.
 */

#ifndef HAS_OGGSYNCMANAGER_H
#define HAS_OGGSYNCMANAGER_H

#include <ogg/ogg.h>
#include <memory>
#include <vector>
#include "../../../include/io/IOHandler.h"

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class OggSyncManager {
public:
    explicit OggSyncManager(PsyMP3::IO::IOHandler* io_handler);
    ~OggSyncManager();

    // Prevent copying
    OggSyncManager(const OggSyncManager&) = delete;
    OggSyncManager& operator=(const OggSyncManager&) = delete;

    /**
     * @brief Feed data from IOHandler into ogg_sync_state
     * @param bytes_requested Number of bytes to try to read
     * @return Number of bytes actually read, or < 0 on error
     */
    long getData(size_t bytes_requested = 4096);

    /**
     * @brief Extract the next page from the sync layer
     * @param[out] page Destination for the parsed page
     * @return 1 if page returned, 0 if more data needed, -1 on stream error
     */
    int getNextPage(ogg_page* page);

    /**
     * @brief Find the page immediately preceding the current file position
     * @param[out] page Destination for the parsed page
     * @return 1 if page found, 0 if not found/error
     */
    int getPrevPage(ogg_page* page);

    /**
     * @brief Find the previous page with a specific serial number
     * @param[out] page Destination for the parsed page
     * @param serial Serial number to match
     * @return 1 if page found, 0 if not found/error
     */
    int getPrevPageSerial(ogg_page* page, long serial);

    /**
     * @brief Reset sync state (e.g., after seek)
     */
    void reset();

    /**
     * @brief Get buffer pointer for writing data directly (advanced usage)
     */
    char* getBuffer(size_t size);

    /**
     * @brief Report bytes written to buffer (advanced usage)
     */
    int wroteBytes(long bytes);

    // Accessors
    PsyMP3::IO::IOHandler* getIOHandler() const { return m_io_handler; }

private:
    PsyMP3::IO::IOHandler* m_io_handler;
    ogg_sync_state m_sync_state;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OGGSYNCMANAGER_H
