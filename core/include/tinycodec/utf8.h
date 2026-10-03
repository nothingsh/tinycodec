#ifndef TINYCODEC_UTF8_H
#define TINYCODEC_UTF8_H

#include <cstddef>
#include <string_view>

namespace tinycodec {

// Returns the length in bytes (1 to 4) of the well-formed UTF-8 sequence at
// cursor, or 0 when it is malformed. Requires cursor < end. Overlong
// encodings, surrogates and code points above U+10FFFF are malformed.
size_t Utf8SequenceLength(const char* cursor, const char* end);

// Returns the length of the longest prefix of text that is well-formed
// UTF-8: text.size() when all of it is, otherwise the offset of the first
// byte of the first malformed sequence.
size_t ValidUtf8Prefix(std::string_view text);

}  // namespace tinycodec

#endif
