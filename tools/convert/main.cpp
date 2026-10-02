// tinycodec-convert: reads JSON from a file or standard input and writes it
// to standard output, reformatted.
//
// Exit codes: 0 success, 1 the input is not valid JSON, 2 bad usage or an
// input/output failure.

#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>

#include "tinycodec/error.h"
#include "tinycodec/json/reader.h"
#include "tinycodec/json/writer.h"
#include "tinycodec/sink.h"

namespace {

const char USAGE[] = "usage: tinycodec-convert [--indent N] [FILE]\n";

// Reads stream to its end. Returns false on a read error.
bool ReadAll(FILE* stream, std::string* out) {
    char buffer[4096];
    for (;;) {
        size_t count = std::fread(buffer, 1, sizeof(buffer), stream);
        out->append(buffer, count);
        if (count < sizeof(buffer)) {
            return std::ferror(stream) == 0;
        }
    }
}

// Parses a whole string as a non-negative int.
bool ParseIndent(const char* text, int* out) {
    const char* end = text + std::strlen(text);
    int value = 0;
    std::from_chars_result result = std::from_chars(text, end, value);
    if (result.ec != std::errc() || result.ptr != end || value < 0) {
        return false;
    }
    *out = value;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    int indent = 2;
    const char* path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--indent") == 0) {
            if (i + 1 >= argc || !ParseIndent(argv[i + 1], &indent)) {
                std::fputs(USAGE, stderr);
                return 2;
            }
            ++i;
        } else if (path == nullptr) {
            path = argv[i];
        } else {
            std::fputs(USAGE, stderr);
            return 2;
        }
    }

    bool useStdin = path == nullptr || std::strcmp(path, "-") == 0;
    const char* name = useStdin ? "<stdin>" : path;

    std::string input;
    bool readOk = false;
    if (useStdin) {
        readOk = ReadAll(stdin, &input);
    } else if (FILE* file = std::fopen(path, "rb")) {
        readOk = ReadAll(file, &input);
        std::fclose(file);
    }
    if (!readOk) {
        std::fprintf(stderr, "tinycodec-convert: cannot read %s\n", name);
        return 2;
    }

    // Reader drives Writer directly; no document is built. The output is
    // collected first so that nothing is printed for invalid input.
    tinycodec::StringSink output;
    tinycodec::json::WriterOptions options;
    options.indent = indent;
    tinycodec::json::Writer writer(output, options);
    tinycodec::json::Reader reader;
    tinycodec::Error error = reader.Parse(input, writer);
    if (!error.Ok()) {
        std::fprintf(stderr, "%s:%d:%d: %s\n", name, error.line, error.column, tinycodec::ErrorName(error.code));
        return 1;
    }

    tinycodec::FileSink standardOutput(stdout);
    if (!standardOutput.Write(output.Str()) || !standardOutput.Write("\n") || std::fflush(stdout) != 0) {
        std::fputs("tinycodec-convert: cannot write the output\n", stderr);
        return 2;
    }
    return 0;
}
