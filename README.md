# ASN.1 Viewer

ASN.1 Viewer is a byte-traceable BER/TLV inspection tool for telecom files.
It is designed to display generic ASN.1 structures first and apply names and
types from supplied ASN.1 specifications when they can be resolved safely.

## Build

Requirements: C++17 and CMake 3.20 or newer. Qt 6 Widgets is optional for the
desktop target; without Qt, the CLI and decoder tests remain available.

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## CLI

```text
asn1viewer decode input.ber
asn1viewer decode input.ber --schema specs/TAP-0312.asn --root TransferBatch

ASN.1 specifications are kept in the `specs/` directory. The CLI and desktop
application also search there automatically for detected schemas.
```

Every decoded node retains its offset, encoded length, tag metadata, raw bytes,
decoded primitive value, and any error status.
