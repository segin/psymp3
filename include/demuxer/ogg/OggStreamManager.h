/*
 * OggStreamManager.h - Ogg Stream Layer (RFC 3533)
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * This class manages a single logical bitstream within an Ogg container.
 * It wraps libogg's ogg_stream_state.
 */

#ifndef HAS_OGGSTREAMMANAGER_H
#define HAS_OGGSTREAMMANAGER_H

#include <ogg/ogg.h>
#include <vector>
#include <map>

namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

class OggStreamManager {
public:
    explicit OggStreamManager(int serial_number);
    ~OggStreamManager();

    // Prevent copying
    OggStreamManager(const OggStreamManager&) = delete;
    OggStreamManager& operator=(const OggStreamManager&) = delete;

    /**
     * @brief Submit a page to this stream
     * @param page The page to submit
     * @return 0 on success, -1 on internal error
     */
    int submitPage(ogg_page* page);

    /**
     * @brief extract a packet from the stream
     * @param[out] packet The extracted packet
     * @return 1 if packet returned, 0 if need more pages
     */
    int getPacket(ogg_packet* packet);

    /**
     * @brief extract a packet from the stream (peek)
     * @param[out] packet The extracted packet
     * @return 1 if packet returned, 0 if need more pages
     */
    int peekPacket(ogg_packet* packet);

    /**
     * @brief Reset stream state
     */
    void reset();

    /**
     * @brief Check if we have gathered all headers
     */
    bool areHeadersComplete() const { return m_headers_complete; }
    void setHeadersComplete(bool complete) { m_headers_complete = complete; }

    int getSerialNumber() const { return m_serial_no; }

private:
    ogg_stream_state m_stream_state;
    int m_serial_no;
    bool m_headers_complete;
    bool m_initialized;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3

#endif // HAS_OGGSTREAMMANAGER_H
