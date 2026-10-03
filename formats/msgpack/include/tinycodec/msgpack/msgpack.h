#ifndef TINYCODEC_MSGPACK_MSGPACK_H
#define TINYCODEC_MSGPACK_MSGPACK_H

#include <string>
#include <string_view>

#include "tinycodec/document.h"
#include "tinycodec/error.h"
#include "tinycodec/msgpack/reader.h"
#include "tinycodec/msgpack/writer.h"
#include "tinycodec/value.h"

namespace tinycodec {
namespace msgpack {

// Parses data into document, replacing whatever it held. On failure the
// document is left empty: document.Root() is null.
Error Parse(std::string_view data, Document& document);

// Returns value as MessagePack, in the shortest encoding. Returns an empty
// string, which is never valid MessagePack, when a string, byte string or
// container is longer than the format allows.
std::string Encode(const Value& value);

}  // namespace msgpack
}  // namespace tinycodec

#endif
