with open('include/codecs/opus/OpusCodec.h', 'r') as f:
    content = f.read()

content = content.replace('    void applyPreSkip_unlocked(AudioFrame& frame);\n', '')

with open('include/codecs/opus/OpusCodec.h', 'w') as f:
    f.write(content)
