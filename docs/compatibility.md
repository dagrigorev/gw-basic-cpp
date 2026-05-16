# GW-BASIC Compatibility Guide

This project is a modern C++20 rewrite, not a byte-for-byte clone of the
original GW-BASIC runtime. The goal is to preserve common source-level behavior
while keeping the implementation portable and testable.

## Running Programs

```bash
gwbasic --file ./examples/snake.bas
gwbasic --headless --file ./examples/snake.bas
gwbasic --check --file ./examples/snake.bas
```

Run without arguments to enter the interactive prompt.

## Supported Language Core

| Area | Supported |
| --- | --- |
| Program model | Line-numbered stored programs, immediate statements, `RUN`, `LIST`, `NEW`, `CLEAR` |
| Assignment | `LET`, implicit assignment, scalar numeric/string variables |
| Expressions | Numeric and string literals, variables, arrays, user `DEF FN` calls, unary `+`/`-`/`NOT`, arithmetic including `^`, integer division `\`, `MOD`, comparisons, `AND`, `OR` |
| Control flow | `IF ... THEN ... ELSE`, line-target `IF`, `GOTO`, `GOSUB`, `RETURN`, `ON ... GOTO`, `ON ... GOSUB` |
| Loops | `FOR ... TO ... STEP` / `NEXT`, `WHILE` / `WEND` |
| Error handling | `ON ERROR GOTO`, `RESUME`, `RESUME NEXT`, `RESUME <line>`, `ERROR`, `ERR`, `ERL` |
| Data | `DATA`, `READ`, `RESTORE` |
| Arrays | `DIM`, `ERASE`, `OPTION BASE 0/1`, one- and multi-dimensional numeric/string arrays |
| User functions | `DEF FNname(args)=expression` for numeric and string expressions |
| Numeric functions | `ABS`, `INT`, `FIX`, `CINT`, `CLNG`, `CSNG`, `CDBL`, `SGN`, `SQR`, `SIN`, `COS`, `TAN`, `ATN`, `EXP`, `LOG`, `RND` |
| String functions | `LEN`, `VAL`, `LEFT$`, `RIGHT$`, `MID$`, `CHR$`, `ASC`, `STR$`, `SPACE$`, `STRING$`, `INSTR`, `UCASE$`, `LCASE$`, `LTRIM$`, `RTRIM$`, `HEX$`, `OCT$`, `DATE$`, `TIME$` |
| Time/random | `TIMER`, `RANDOMIZE`, `RND` |
| Default typing | `DEFINT`, `DEFSTR`, `DEFSNG`, `DEFDBL` |

## Supported I/O and Files

| Area | Supported |
| --- | --- |
| Console | `PRINT`, `WRITE`, `INPUT`, `LINE INPUT`, `INKEY$`, `INPUT$` |
| Cursor/color | `CLS`, `LOCATE`, `WIDTH`, `COLOR`, `KEY ON/OFF` |
| Sequential files | `OPEN ... FOR INPUT/OUTPUT/APPEND`, `CLOSE`, `INPUT#`, `LINE INPUT#`, `PRINT#`, `WRITE#`, `EOF`, `LOF`, `LOC` |
| Random files | `OPEN ... FOR RANDOM LEN=`, `FIELD`, `LSET`, `RSET`, `GET#`, `PUT#` |
| Filesystem | Text `SAVE`, text `LOAD`, `FILES`, `KILL`, `NAME ... AS`, `MKDIR`, `CHDIR`, `RMDIR` |

## Supported Graphics and Sound Surface

| Area | Supported |
| --- | --- |
| Screen | `SCREEN`, headless canvas, aspect-preserving Direct3D presenter on Windows, aspect-preserving X11/OpenGL presenter on Linux |
| Drawing | `PSET`, graphics `LINE`, `CIRCLE`, `PAINT`, `DRAW` |
| View transforms | `VIEW`, `WINDOW`, `PMAP`, `POINT` |
| Blocks | Graphics `GET` / `PUT`, `PSET`, `PRESET`, `AND`, `OR`, `XOR` modes |
| Palette | `PALETTE`, `PALETTE USING` |
| Sound | `BEEP`, stubbed `SOUND`, stubbed `PLAY` |

## Memory Compatibility

`POKE` and `PEEK` are implemented against an isolated 64 KiB virtual memory
buffer. They never write to native process memory. Addresses outside `0..65535`
raise a runtime error and can be handled with `ON ERROR`.

## Known Differences

- Numeric behavior uses modern `double`; it does not emulate 8087 or Microsoft
  BASIC floating-point quirks exactly.
- `SOUND` and `PLAY` currently produce bell-style output rather than PC speaker
  synthesis.
- `WIDTH` tracks internal text width for the portable text/graphics layer; it
  does not resize the host terminal.
- Binary tokenized `.BAS` files are not decoded yet; `SAVE` and `LOAD`
  currently use text BASIC source.
- DOS drive/device semantics are only approximated through portable filesystem
  operations. `FILES` supports simple `*` and `?` wildcards, not the full DOS
  device/path model.
- The original GW-BASIC editor experience is not reproduced.

## Test Coverage

CTest currently includes:

- Lexer unit tests.
- Parser/AST unit tests.
- Interpreter compatibility golden-output tests.
- Smoke coverage for files, graphics, arrays, input, palette, and runtime flow.

Optional CI targets also include static analysis and a libFuzzer smoke target
for lexer/parser robustness.

## Generated API Documentation

The repository can build Doxygen HTML documentation with:

```bash
cmake -S . -B build-docs -DGWBASIC_BUILD_DOCS=ON
cmake --build build-docs --target docs
```

Pull requests build this documentation as a check. Pushes to `main` publish the
generated HTML through GitHub Pages.
