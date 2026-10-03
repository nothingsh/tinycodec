// tinycodec-convert: reads JSON, MessagePack or TOML from a file or standard
// input and writes it to standard output, as JSON (reformatted), MessagePack
// or TOML.
//
// Exit codes: 0 success, 1 the input is not valid or cannot be written in
// the output format, 2 bad usage or an input/output failure.

#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <system_error>

#include "tinycodec/error.h"
#include "tinycodec/json/reader.h"
#include "tinycodec/json/writer.h"
#include "tinycodec/msgpack/reader.h"
#include "tinycodec/msgpack/writer.h"
#include "tinycodec/sink.h"
#include "tinycodec/toml/reader.h"
#include "tinycodec/toml/writer.h"
#include "tinycodec/visitor.h"

namespace {

const char USAGE[] = "usage: tinycodec-convert [--from FORMAT] [--to FORMAT] [--indent N] [FILE]\n";

enum class Format { Json, Msgpack, Toml };

const char* FormatName(Format format) {
    switch (format) {
    case Format::Json:    return "json";
    case Format::Msgpack: return "msgpack";
    case Format::Toml:    return "toml";
    }
    return "";
}

bool ParseFormat(const char* text, Format* out) {
    if (std::strcmp(text, "json") == 0) {
        *out = Format::Json;
        return true;
    }
    if (std::strcmp(text, "msgpack") == 0) {
        *out = Format::Msgpack;
        return true;
    }
    if (std::strcmp(text, "toml") == 0) {
        *out = Format::Toml;
        return true;
    }
    return false;
}

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
    Format from = Format::Json;
    Format to = Format::Json;
    int indent = 2;
    const char* path = nullptr;
    for (int i = 1; i < argc; ++i) {
        bool ok = true;
        if (std::strcmp(argv[i], "--indent") == 0) {
            ok = i + 1 < argc && ParseIndent(argv[i + 1], &indent);
            ++i;
        } else if (std::strcmp(argv[i], "--from") == 0) {
            ok = i + 1 < argc && ParseFormat(argv[i + 1], &from);
            ++i;
        } else if (std::strcmp(argv[i], "--to") == 0) {
            ok = i + 1 < argc && ParseFormat(argv[i + 1], &to);
            ++i;
        } else if (path == nullptr) {
            path = argv[i];
        } else {
            ok = false;
        }
        if (!ok) {
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
    std::unique_ptr<tinycodec::Visitor> writer;
    if (to == Format::Json) {
        tinycodec::json::WriterOptions options;
        options.indent = indent;
        writer = std::make_unique<tinycodec::json::Writer>(output, options);
    } else if (to == Format::Msgpack) {
        writer = std::make_unique<tinycodec::msgpack::Writer>(output);
    } else {
        writer = std::make_unique<tinycodec::toml::Writer>(output);
    }

    tinycodec::Error error;
    if (from == Format::Json) {
        error = tinycodec::json::Reader().Parse(input, *writer);
    } else if (from == Format::Msgpack) {
        error = tinycodec::msgpack::Reader().Parse(input, *writer);
    } else {
        error = tinycodec::toml::Reader().Parse(input, *writer);
    }
    if (!error.Ok()) {
        // Aborted means the writer refused a value that the output format
        // cannot express.
        std::string problem = error.code == tinycodec::ErrorCode::Aborted
            ? std::string("cannot be written as ") + FormatName(to)
            : tinycodec::ErrorName(error.code);
        if (from != Format::Msgpack) {
            std::fprintf(stderr, "%s:%d:%d: %s\n", name, error.line, error.column, problem.c_str());
        } else {
            std::fprintf(stderr, "%s: offset %zu: %s\n", name, error.offset, problem.c_str());
        }
        return 1;
    }

    // TOML output already ends each line; MessagePack output is binary.
    tinycodec::FileSink standardOutput(stdout);
    bool written = standardOutput.Write(output.Str());
    if (to == Format::Json) {
        written = written && standardOutput.Write("\n");
    }
    if (!written || std::fflush(stdout) != 0) {
        std::fputs("tinycodec-convert: cannot write the output\n", stderr);
        return 2;
    }
    return 0;
}
