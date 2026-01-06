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
#include <vector>
#include <cstdint>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

/**
 * @brief Thread-safe and lifetime-safe container for an Ogg page.
 * Holds its own copies of header and body data.
 */
struct SafeOggPage {
    std::vector<unsigned char> header;
    std::vector<unsigned char> body;
    ogg_page page; // Pointers points into the vectors above

    SafeOggPage() {
        page.header = nullptr;
        page.header_len = 0;
        page.body = nullptr;
        page.body_len = 0;
    }

    void clone(const ogg_page* src) {
        if (!src) return;
        header.assign(src->header, src->header + src->header_len);
        body.assign(src->body, src->body + src->body_len);
        page.header = header.data();
        page.header_len = header.size();
        page.body = body.data();
        page.body_len = body.size();
    }
};

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
     * @brief Extract the next page and deep-copy its data
     * @param[out] safe_page Destination for the deep-copied page
     * @return 1 if page returned, 0 if more data needed, -1 on error
     */
    int getNextPage(SafeOggPage* safe_page);

    /**
     * @brief Find the file offset of the page immediately preceding the current file position
     * @return File offset of the page header, or -1 if not found/error
     */
    int64_t findPrevPage();

    /**
     * @brief Find the file offset of the previous page with a specific serial number
     * @param serial Serial number to match
     * @return File offset of the page header, or -1 if not found/error
     */
    int64_t findPrevPageSerial(long serial);

    /**
     * @brief Reset sync state (e.g., after seek)
     */
    void reset();

    /**
     * @brief Get the current logical file position of the next unconsumed byte
     */
    int64_t getLogicalPosition() const { return m_logical_offset; }

    /**
     * @brief Get buffer pointer for writing data directly (advanced usage)
     */
    char* getBuffer(size_t size);

    /**
     * @brief Report bytes written to buffer (advanced usage)
     */
    int wroteBytes(long bytes);

    // File position and seeking
    int64_t getPosition() const;
    int64_t getFileSize();
    bool seek(int64_t position);

    // Accessors
    PsyMP3::IO::IOHandler* getIOHandler() const { return m_io_handler; }

private:
    PsyMP3::IO::IOHandler* m_io_handler;
    ogg_sync_state m_sync_state;
    int64_t m_logical_offset;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OGGSYNCMANAGER_H
