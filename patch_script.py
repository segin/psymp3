import re

with open('src/codecs/opus/OpusCodec.cpp', 'r') as f:
    content = f.read()

# Replace assignment logic
orig_assign = """    // Create AudioFrame from decoded samples
    frame.sample_rate = 48000;
    frame.channels = m_channels;
    frame.samples.assign(m_output_buffer.begin(), m_output_buffer.begin() + total_samples);

    return frame;"""

new_assign = """    // Calculate pre-skip offset
    uint64_t samples_to_skip = m_samples_to_skip.load();
    size_t actual_skip_frames = 0;

    if (samples_to_skip > 0 && samples_decoded > 0) {
        actual_skip_frames = std::min(static_cast<size_t>(samples_decoded), static_cast<size_t>(samples_to_skip));

        uint64_t expected = samples_to_skip;
        while (!m_samples_to_skip.compare_exchange_weak(expected, expected - actual_skip_frames)) {
            if (expected < actual_skip_frames) {
                actual_skip_frames = expected;
                break;
            }
        }

        Debug::log("opus", "Pre-skip applied during decoding: skipped ", actual_skip_frames, " sample frames");
    }

    size_t frames_to_keep = samples_decoded - actual_skip_frames;
    size_t samples_to_keep = frames_to_keep * m_channels;
    size_t offset_samples = actual_skip_frames * m_channels;

    // Create AudioFrame from decoded samples
    frame.sample_rate = 48000;
    frame.channels = m_channels;

    if (samples_to_keep > 0) {
        frame.samples.assign(m_output_buffer.begin() + offset_samples,
                             m_output_buffer.begin() + offset_samples + samples_to_keep);
    }

    return frame;"""

content = content.replace(orig_assign, new_assign)

# Remove call to applyPreSkip_unlocked
orig_call = """    // Normal decoding
    AudioFrame frame = decodeAudioPacket_unlocked(chunk.data);

    applyPreSkip_unlocked(frame);
    applyOutputGain_unlocked(frame);"""

new_call = """    // Normal decoding
    AudioFrame frame = decodeAudioPacket_unlocked(chunk.data);

    applyOutputGain_unlocked(frame);"""

content = content.replace(orig_call, new_call)

# Remove the applyPreSkip_unlocked function completely
# We'll find its start and end
start_idx = content.find('void OpusCodec::applyPreSkip_unlocked(AudioFrame& frame)')
if start_idx != -1:
    end_idx = content.find('void OpusCodec::applyOutputGain_unlocked(AudioFrame& frame)')
    content = content[:start_idx] + content[end_idx:]

with open('src/codecs/opus/OpusCodec.cpp', 'w') as f:
    f.write(content)
