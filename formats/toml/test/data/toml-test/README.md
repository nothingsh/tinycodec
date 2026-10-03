These files are the TOML 1.0.0 tests of
[toml-test](https://github.com/toml-lang/toml-test), copied unchanged from
commit `ff49d109861c1ad25af53f687f2aef19ab650600`. Only the files listed in
`files-toml-1.0.0` (copied from `tests/files-toml-1.0.0`) are included,
under the same relative paths. See `LICENSE` for the terms.

- `valid/**/X.toml` must be accepted, and must decode to what
  `valid/**/X.json` describes: tables as JSON objects, arrays as JSON
  arrays, and every other value as `{"type": T, "value": V}`, where `T` is
  `string`, `integer`, `float`, `bool`, `datetime`, `datetime-local`,
  `date-local` or `time-local` and `V` is the value written as a string.
- `invalid/**/*.toml` must be rejected.
