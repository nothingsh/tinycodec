`msgpack-test-suite.json` is `dist/msgpack-test-suite.json` of
[msgpack-test-suite](https://github.com/kawanet/msgpack-test-suite) by
Yusuke Kawasaki, copied unchanged from commit
`e04f6edeaae589c768d6b70fcce80aa786b7800e`. See `LICENSE` for the terms.

It maps a group name to a list of cases. Each case has one field giving the
expected value (`nil`, `bool`, `binary`, `number`, `bignum`, `string`,
`array`, `map`, `timestamp` or `ext`) and a `msgpack` field listing
equivalent encodings, each written as hex bytes separated by `-`.
