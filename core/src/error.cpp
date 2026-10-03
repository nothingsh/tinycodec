#include "tinycodec/error.h"

namespace tinycodec {

const char* ErrorName(ErrorCode code) {
    switch (code) {
    case ErrorCode::Ok:             return "Ok";
    case ErrorCode::UnexpectedEnd:  return "UnexpectedEnd";
    case ErrorCode::UnexpectedChar: return "UnexpectedChar";
    case ErrorCode::InvalidNumber:  return "InvalidNumber";
    case ErrorCode::InvalidEscape:  return "InvalidEscape";
    case ErrorCode::InvalidUtf8:    return "InvalidUtf8";
    case ErrorCode::DepthExceeded:  return "DepthExceeded";
    case ErrorCode::Unsupported:    return "Unsupported";
    case ErrorCode::Aborted:        return "Aborted";
    }
    return "Unknown";
}

}  // namespace tinycodec
