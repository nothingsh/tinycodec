#include "tinycodec/utf8.h"

namespace tinycodec {

namespace {

bool IsContinuation(unsigned char byte) {
    return (byte & 0xC0) == 0x80;
}

}  // namespace

size_t Utf8SequenceLength(const char* cursor, const char* end) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(cursor);
    size_t available = static_cast<size_t>(end - cursor);
    unsigned char lead = bytes[0];
    if (lead < 0x80) {
        return 1;
    }

    size_t length = 0;
    unsigned char secondMin = 0x80;
    unsigned char secondMax = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        if (lead == 0xE0) {
            secondMin = 0xA0;  // Below this the encoding would be overlong.
        } else if (lead == 0xED) {
            secondMax = 0x9F;  // Above this lie the surrogates.
        }
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        if (lead == 0xF0) {
            secondMin = 0x90;  // Below this the encoding would be overlong.
        } else if (lead == 0xF4) {
            secondMax = 0x8F;  // Above this lies everything past U+10FFFF.
        }
    } else {
        return 0;  // A continuation byte, 0xC0, 0xC1, or 0xF5 and above.
    }

    if (available < length || bytes[1] < secondMin || bytes[1] > secondMax) {
        return 0;
    }
    for (size_t i = 2; i < length; ++i) {
        if (!IsContinuation(bytes[i])) {
            return 0;
        }
    }
    return length;
}

size_t ValidUtf8Prefix(std::string_view text) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const char* cursor = begin;
    while (cursor != end) {
        size_t length = Utf8SequenceLength(cursor, end);
        if (length == 0) {
            break;
        }
        cursor += length;
    }
    return static_cast<size_t>(cursor - begin);
}

}  // namespace tinycodec
