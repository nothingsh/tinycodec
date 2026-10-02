#include "tinycodec/sink.h"

namespace tinycodec {

bool StringSink::Write(std::string_view data) {
    _buffer.append(data.data(), data.size());
    return true;
}

FileSink::FileSink(FILE* file) : _file(file) {}

bool FileSink::Write(std::string_view data) {
    if (data.empty()) {
        return true;
    }
    return std::fwrite(data.data(), 1, data.size(), _file) == data.size();
}

}  // namespace tinycodec
