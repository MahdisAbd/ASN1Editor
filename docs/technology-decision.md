# Technology Decision

## Selected stack

- C++17
- Qt 6 Widgets for the desktop UI
- CMake for builds
- A small custom BER/TLV decoder for exact byte ranges and error recovery
- A runtime ASN.1 schema index/parser for supplied specifications

## Rationale

C++ provides direct control over byte ranges and memory while Qt's model/view
widgets match the hierarchical viewer. The generic decoder needs offsets,
partial recovery, and raw encoded ranges that are not naturally exposed by a
generated-code-only runtime. Generated ASN.1 tooling may still be used as an
external comparison tool.

The project is configured so the CLI and core library build without Qt. The UI
target is enabled automatically when Qt 6 is found.

## Deferred alternatives

Go was not selected because runtime loading of the supplied ASN.1 modules and a
full schema-aware BER decoder would require substantially more custom tooling.
C was not selected because the tree model, diagnostics, and desktop UI would
have a higher implementation cost without a material product benefit.
