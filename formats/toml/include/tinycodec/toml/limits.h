#ifndef TINYCODEC_TOML_LIMITS_H
#define TINYCODEC_TOML_LIMITS_H

namespace tinycodec {
namespace toml {

// Tables and arrays may be nested at most this deep, counting the root
// table as the first level. The Reader rejects deeper input, and the
// Writer deeper events, so whatever the Writer writes the Reader can read.
constexpr int kMaxDepth = 500;

}  // namespace toml
}  // namespace tinycodec

#endif
