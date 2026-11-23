# ash

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CMake 3.25+](https://img.shields.io/badge/CMake-3.25%2B-064F8C.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-orange.svg)](https://github.com/abidanBrito/ash/blob/main/LICENSE)

A small POSIX-like shell written in C++23.

## Features

- Builtins: `echo`, `type`, `pwd`, `cd`, `history`, `exit`.
- Running external programs found on `PATH`.
- Pipelines.
- Output redirection.
- Single/double quoting and backslash escapes.
- Line editing, history and tab completion.
- Persistent history.

## Requirements

- GCC 11+ or Clang 17+.
- CMake 3.25+.
- Ninja (optional, only needed if you build with the CMake presets).
- GNU Readline and its headers.
- `pkg-config`.

> [!NOTE]
> Only Unix-like systems are supported for now.

## Build & run

### With presets

```sh
cmake --preset clang-release
cmake --build --preset clang-release

./build/clang-release/ash
```

Presets are named `<compiler>-<variant>`, with `clang` or `gcc` as the compiler:

| Variant   | Description                     |
| --------- | ------------------------------- |
| `debug`   | Debug build                     |
| `release` | Release build                   |
| `asan`    | Debug build with ASan and UBSan |

> [!TIP]
> `cmake --list-presets` and `cmake --build --list-presets` show all available CMake presets.

### Without presets

Use any generator that CMake supports:

```sh
cmake -B build
cmake --build build

./build/ash
```

## Roadmap

Not yet implemented:

- [ ] Track exit statuses.
- [ ] Signal handling.
- [ ] Multi-line input.
- [ ] Input redirection and `2>&1`.
- [ ] Variables: assignment & expansion.
- [ ] Tilde expansion and globbing.
- [ ] Command substitution.
- [ ] Command lists (`;`, `&&`, `||`).
- [ ] Environment (`export`, `unset`).
- [ ] Aliases (`alias`, `unalias`).
- [ ] Background jobs and job control (`&`, `jobs`, `fg`, `bg`).
- [ ] Compound commands (`if`, `while`, `for`).
- [ ] Scripts, `-c` and comments.

## License

This repository is released under the MIT license. See [MIT](LICENSE) for details.
