# tinycodec

A small C++17 library for decoding and encoding tree-shaped data formats,
in the spirit of [tinyxml2](https://github.com/leethomason/tinyxml2).

tinycodec is a learning project: the goals are a clear parser
implementation, a format-independent interface and a clean multi-target
build. Performance is good enough, not a goal.

## Supported formats

| Format                                         | Target               | Decode | Encode | Notes                                            |
|------------------------------------------------|----------------------|:------:|:------:|--------------------------------------------------|
| [JSON](https://www.rfc-editor.org/rfc/rfc8259) | `tinycodec::json`    | ✓      | ✓      | Strict RFC 8259, no extensions                   |
| [MessagePack](https://msgpack.org)             | `tinycodec::msgpack` | ✓      | ✓      | `ext` types (including timestamps) not supported |

More formats will be added over time; each one is a separate target, so you
only link the formats you use.

## Design

Every format talks to the rest of the library through one event-based
`Visitor` interface: scalars, plus entering and leaving arrays and objects.

- A format's **Reader** parses input and sends events to a Visitor.
- A format's **Writer** is a Visitor that turns events into output.
- `DocumentBuilder` is a Visitor that builds a `Document` tree, and
  `Value::Accept` replays a tree as events.

Because readers and writers share the same interface, a reader of one format
can drive a writer of another directly, with no tree in between.

Other properties:

- No exceptions; errors are reported as an `Error` with a code, byte offset,
  and (for text formats) line and column.
- Values live in a bump arena owned by their `Document`.
- Nesting is limited to 500 levels.
- Each format is its own CMake target, so you only link what you use.

| Target                | Contents                                                       |
|-----------------------|----------------------------------------------------------------|
| `tinycodec::core`     | `Visitor`, `Value`, `Document`, `DocumentBuilder`, `Sink`, `Error` |
| `tinycodec::json`     | `json::Reader`, `json::Writer`, `json::Parse`, `json::Stringify` |
| `tinycodec::msgpack`  | `msgpack::Reader`, `msgpack::Writer`, `msgpack::Parse`, `msgpack::Encode` |

## Usage

### Parse into a tree

```cpp
#include "tinycodec/json/json.h"
#include "tinycodec/msgpack/msgpack.h"

tinycodec::Document document;
tinycodec::Error error = tinycodec::json::Parse(R"({"answer": [42]})", document);
if (!error.Ok()) {
    std::printf("%s at line %d, column %d\n",
                tinycodec::ErrorName(error.code), error.line, error.column);
    return;
}

int64_t answer = 0;
document.Root()->Find("answer")->At(0)->QueryInt(&answer);   // 42

std::string json = tinycodec::json::Stringify(*document.Root(), {/*indent=*/2});
std::string packed = tinycodec::msgpack::Encode(*document.Root());
```

### Convert without building a tree

```cpp
#include "tinycodec/json/reader.h"
#include "tinycodec/msgpack/writer.h"
#include "tinycodec/sink.h"

tinycodec::StringSink sink;
tinycodec::msgpack::Writer writer(sink);
tinycodec::json::Reader reader;
tinycodec::Error error = reader.Parse(R"([1, "two", null])", writer);
// sink.Str() now holds the MessagePack encoding.
```

### Data model notes

- Integers are kept exactly: `Int` for anything that fits in `int64_t`,
  `Uint` above that, `Double` otherwise.
- MessagePack `bin` values become `Bytes`. JSON cannot represent `Bytes`,
  so writing them as JSON fails.
- MessagePack `ext` values (including timestamps) and non-string map keys
  are rejected with `ErrorCode::Unsupported`.
- JSON extensions (comments, trailing commas, single quotes, BOM) are not
  accepted.

## Building

Requirements: CMake 3.20+ and a C++17 compiler whose standard library
provides `std::from_chars` / `std::to_chars` for `double` (with Apple clang,
a deployment target of macOS 26 or later). Configuration fails with a clear
message otherwise.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Options (both default to `ON` when tinycodec is the top-level project):

- `TINYCODEC_BUILD_TESTS`: build the unit, conformance and install tests.
- `TINYCODEC_BUILD_TOOLS`: build `tinycodec-convert`.

### Using it from another project

After `cmake --install build --prefix <dir>`:

```cmake
find_package(tinycodec 0.1 REQUIRED COMPONENTS json msgpack)
target_link_libraries(app PRIVATE tinycodec::json tinycodec::msgpack)
```

Or add the source tree with `add_subdirectory(tinycodec)`; tests and tools
are then off by default.

## Command line tool

`tinycodec-convert` reads JSON or MessagePack from a file or standard input
and writes JSON (reformatted) or MessagePack to standard output.

```sh
tinycodec-convert [--from json|msgpack] [--to json|msgpack] [--indent N] [FILE]

tinycodec-convert --indent 4 data.json                  # pretty-print
tinycodec-convert --to msgpack data.json > data.msgpack
tinycodec-convert --from msgpack data.msgpack            # back to JSON
```

Both formats default to `json` and the indent defaults to 2 (`0` writes
compact output). Exit codes: `0` success, `1` invalid input or output that
the target format cannot express, `2` bad usage or an I/O failure.

## Tests

Tests use [doctest](https://github.com/doctest/doctest). Conformance is
checked against [JSONTestSuite](https://github.com/nst/JSONTestSuite) and
[msgpack-test-suite](https://github.com/kawanet/msgpack-test-suite), whose
data is vendored under `formats/*/test/data`.

## License

[MIT](LICENSE). Vendored third-party code and test data keep their own
licenses (all MIT); see `third_party/doctest` and the `LICENSE` files next to
the test data.
