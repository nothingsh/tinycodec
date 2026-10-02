#ifndef TINYCODEC_SINK_H
#define TINYCODEC_SINK_H

#include <cstdio>
#include <string>
#include <string_view>

namespace tinycodec {

// Where a writer sends its output.
class Sink {
public:
    virtual ~Sink() = default;

    // Returns false when the data could not be written.
    virtual bool Write(std::string_view data) = 0;
};

// Collects the output in memory.
class StringSink : public Sink {
public:
    bool Write(std::string_view data) override;
    const std::string& Str() const { return _buffer; }

private:
    std::string _buffer;
};

// Writes to a C stream. The stream is not owned and is never closed here.
class FileSink : public Sink {
public:
    explicit FileSink(FILE* file);
    bool Write(std::string_view data) override;

private:
    FILE* _file;
};

}  // namespace tinycodec

#endif
