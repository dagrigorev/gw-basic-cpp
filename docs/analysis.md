# GW-BASIC source analysis

## What this code is

The archive is the original Microsoft GW-BASIC interpreter source code from 1983. It is **not** a normal application with a small entry point and clean module boundaries. It is a large 8086 assembly codebase for a line-oriented BASIC interpreter tightly coupled to MS-DOS era memory layout, tokenization rules, device drivers, and OEM-specific hardware abstractions.

## High-level structure found in the source

- `GWMAIN.ASM`: interpreter core, statement dispatch, error paths, program execution plumbing.
- `GWEVAL.ASM`: expression parser and evaluator.
- `GWLIST.ASM`: listing, editing, and line-management behavior.
- `GWDATA.ASM`: data/file/runtime support and many shared declarations.
- `GWRAM.ASM`: global RAM layout and initialized interpreter state.
- `MATH1.ASM`, `MATH2.ASM`: numeric parsing, formatting, and arithmetic helper routines.
- `IBMRES.ASM`, `IBMRES.H`, `BINTRP.H`: token/reserved-word tables and macro machinery.
- `GIO*.ASM`, `SCN*.ASM`, `DSK*.ASM`: console/screen/disk/printer/serial and device abstractions.
- `ADVGRP.ASM`, `BIPRTU.ASM`, `BIPTRG.ASM`, etc.: graphics, printer, and OEM/platform behavior.

## Architectural characteristics of the original code

1. **Token-dispatch interpreter**
   - Reserved words are mapped to numeric tokens and dispatch tables.
   - Statements and functions are selected through table-driven assembly entry points.

2. **Global mutable state everywhere**
   - The runtime uses global memory blocks for variables, text storage, stacks, temporaries, FAC/DFAC numeric accumulators, line pointers, and error state.
   - This is efficient for 1983 constraints, but hostile to modularity and testing.

3. **Mixed concerns**
   - Parsing, evaluation, runtime, device I/O, and memory management are heavily intertwined.
   - Hardware and OEM concerns leak into core execution logic.

4. **Representation optimized for constrained hardware**
   - Packed tokens, hand-written numeric conversions, custom string and heap handling, device control blocks, and BIOS/DOS assumptions.

## Rewrite strategy

A modern rewrite should **preserve the behavior model**, not the assembly structure.

### What should be preserved

- Line-numbered program model.
- BASIC-style immediate mode and stored program mode.
- Core statement semantics.
- Integer/float/string value handling.
- Deterministic interpreter execution.

### What should be replaced

- Global RAM layout -> explicit runtime/context objects.
- Token tables in assembly -> typed enums and maps.
- Direct device/hardware code -> portable console/device interfaces.
- Ad hoc parser -> recursive descent parser over a real token stream.
- Implicit control flow -> explicit program counter and execution frames.

## Recommended phased plan

### Phase 1: portable core
- Lexer
- Parser
- AST
- Runtime context
- Program storage by line number
- Interpreter loop
- Minimal console I/O

### Phase 2: language coverage
- Arrays, DATA/READ/RESTORE
- DEFINT/DEFSTR/DEFSNG/DEFDBL
- User functions
- WHILE/WEND, ON GOTO/GOSUB
- String functions and numeric intrinsics

### Phase 3: compatibility work
- Original tokenization quirks
- PRINT formatting behavior
- GW-BASIC error codes/messages
- File commands and device names
- More accurate numeric semantics

### Phase 4: platform abstraction
- Screen abstraction
- Printer/serial/file device adapters
- Optional graphics subsystem
- Optional compatibility mode per target platform

## What this rewrite currently implements

This rewritten project is a **clean cross-platform C++20 foundation**, not a full binary-compatible clone.

Implemented:
- `PRINT`
- `LET` and implicit assignment
- `INPUT`
- `IF ... THEN <line>`
- `GOTO`, `GOSUB`, `RETURN`
- `FOR ... TO ... STEP`, `NEXT`
- `END`, `REM`, `LIST`, `RUN`, `NEW`, `CLEAR`
- Expression parsing with strings, numbers, variables, arithmetic, and comparisons
- Stored line-numbered programs and immediate execution mode
- CMake build and a smoke test

Not yet implemented:
- Full GW-BASIC statement set
- DATA/READ/RESTORE
- Arrays
- File and device subsystems
- Graphics and sound
- Original memory model and binary compatibility


## v18 extension

Added a portable in-memory graphics compatibility layer beyond simple pixel/line primitives: flood fill (`PAINT`), command-string vector drawing (`DRAW`), and viewport clipping (`VIEW`). This keeps the rewrite cross-platform and testable without adding a GUI dependency yet.


Latest extension: added portable graphics block capture/blit on the in-memory canvas using BASIC-style `GET`/`PUT` graphics forms, kept distinct from file `GET#`/`PUT#`.


## Latest extension

The graphics subsystem now supports BASIC-style block blit modes for `PUT`, which is an important step toward sprite-like behavior and region composition. The rewrite keeps this as a portable in-memory canvas feature rather than depending on a platform renderer.


Recent extension: added `PALETTE USING` on top of the portable palette remap table. In this rewrite it accepts a one-dimensional numeric array and loads up to 256 remap entries from it.


Latest update: v24 adds a separate nonblocking key-input path so `INKEY$` can poll the terminal without forcing line-buffered `getline()` behavior. On Windows it uses `_kbhit()`/`_getch()`. On POSIX terminals it temporarily switches stdin to noncanonical no-echo mode, uses `select()`, reads one byte if available, and restores the previous terminal settings. When no key backend is configured, the runtime still falls back to the older line-based behavior for tests and embedding scenarios.
