# libkw

Header-only C++26 library for Python-style keyword arguments.

```cpp
#include "libkw.hpp"

using libkw::kw;

auto add3(int x, int y, int z) { return x + y + z; }

KW_CALL(add3, kw<"z">, 3, kw<"x">, 1, kw<"y">, 2); // == add3(1, 2, 3)
```

Zero cost: with mild optimization (`-O2` is sufficient), generates identical
assembly to a hand-written positional call. When the callee is inlinable,
the two are byte-identical (*).

(*) Under `[[gnu::noinline]]` or `[[gnu::noipa]]`, the assembly is *not*
byte-identical — it matches only up to stack slot reordering.

## Limitations

- `KW_CALL`/`KW_METHOD_CALL` cannot target an overloaded function or method.
  `^^f` requires `f` to already be an unambiguous id-expression; there's no
  cast-based workaround (`^^` rejects cast expressions outright, and
  reflecting a disambiguating function-reference variable reflects the
  variable, not the function). Give the target a unique name, or wrap it in
  a uniquely-named forwarding function.

## Dependencies

- A C++26 compiler with `-freflection` (P2996) support — e.g. GCC 16+ trunk.
  Not yet supported by stable GCC/Clang releases.
- CMake >= 4.3, Ninja.
- Python 3, for the test suites.
- clang-format and ruff, for formatting (`format.bash`).

## Build & test

```bash
./test.bash
```

Runs four suites:
- `test_basic` — functional correctness: argument matching/reordering,
  value-category (copy/move) preservation, free functions and methods.
- `test_standalone_include` — compiles `libkw.hpp` as the only `#include`,
  guarding against relying on a transitively-included header.
- `expect_failure.py` — compiles each `test/expect_failure/*.cpp` case and
  checks the compiler error matches the expected `.oracle` output.
- `modal.py` — the zero-cost assembly comparison described above.

## License

BSD 3-Clause, see [LICENSE](LICENSE).
