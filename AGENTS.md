# ASN.1 Viewer — Project Context

## Project Goal

Build a lightweight desktop application for inspecting and decoding binary telecom files encoded using ASN.1.

The application must support:

- General ASN.1 binary files
- TAP telecom files
- Different ASN.1 schemas/specifications supplied with the project

The primary goal is to turn difficult-to-read binary data into a clear, accurate, human-readable tree.

---

## Core Requirements

### 1. Binary File Reading

The viewer must read the input file directly from its binary representation.

Parsing should be performed carefully and deterministically from the actual bytes in the file.

The decoder should correctly identify and process information such as:

- Byte offsets
- Tag class
- Tag number
- Constructed/primitive state
- Length
- Raw value
- Decoded value
- Nested structures

Do not make assumptions about field boundaries without validating them against the ASN.1 encoding rules or the available specification.

The parser should be able to trace the file from the first byte to the last meaningful byte.

---

### 2. ASN.1 Specification Support

I will provide ASN.1 specification files for several telecom formats inside the project directory.

The application should inspect and use these specifications whenever possible to correctly determine:

- Object names
- Field names
- Data types
- Nested structures
- SEQUENCE structures
- SET structures
- CHOICE structures
- ENUMERATED values
- INTEGER values
- OCTET STRING values
- BIT STRING values
- BOOLEAN values
- Object identifiers
- Context-specific tags
- Application-specific tags
- Other ASN.1 structures encountered in the supplied specifications

The architecture should not be tightly coupled to a single ASN.1 specification.

Adding another ASN.1 specification later should require minimal or no code changes.

---

### 3. Generic ASN.1 Viewer

For ordinary ASN.1 files, present the decoded information as a clean hierarchical tree.

A node may conceptually contain information such as:

`Tag 3`
- Type: Context Specific
- Constructed: Yes
- Length: 42
- Offset: 128
- Value:
  - Tag 1
  - Tag 2
  - Tag 5

Leaf nodes should show useful information such as:

`Tag 4`
- Type: INTEGER
- Offset: 152
- Length: 4
- Raw: `00 01 A4 32`
- Value: `107570`

Do not clutter the main interface with unnecessary technical information.

Advanced/raw information can be available when the user expands a node or opens its details.

---

## TAP File Support

TAP files require special treatment because meaningful ASN.1 object and field names are important when inspecting telecom records.

When a TAP specification is available, the viewer should map binary structures to their proper ASN.1 names.

For example, instead of presenting only:

`Tag 5 -> Tag 2 -> Tag 7`

the viewer should preferably present meaningful structures such as:

`TransferBatch`
- `CallEventDetails`
  - `MobileOriginatedCall`
    - `LocationInformation`
    - `CellId`
    - `CalledNumber`

The actual names must come from the supplied specification and must not be invented.

TAP-specific presentation should remain separate from the generic ASN.1 decoding logic.

---

## Human-Readable Values

Raw ASN.1 values should be converted into useful representations whenever the interpretation is known.

Examples include:

- Binary INTEGER -> decimal integer
- BOOLEAN -> `true` / `false`
- Encoded strings -> readable text
- ENUMERATED -> symbolic name where available
- OCTET STRING -> readable representation when its type is known
- Binary identifiers -> normalized values
- Dates and timestamps -> readable date/time
- Telecom identifiers -> properly formatted values when the specification defines their encoding

Where useful, preserve both:

`Decoded value`

and

`Raw bytes`

so the decoded result can always be verified against the source data.

Never silently alter a value merely to make it look cleaner.

---

## Tree Structure

The main interface should be centered around a tree view.

Example:

`TransferBatch`
- `BatchControlInfo`
  - `Sender`
  - `Recipient`
  - `FileCreationTimestamp`
- `CallEventDetails`
  - `[0] MobileOriginatedCall`
    - `CalledNumber`
    - `CallingNumber`
    - `LocationInformation`
      - `LocationAreaCode`
      - `CellId`
    - `CallDuration`
  - `[1] MobileTerminatedCall`
    - ...

The tree should make deeply nested ASN.1 structures easy to explore.

Useful operations include:

- Expand/collapse nodes
- Expand all / collapse all
- Search
- Copy value
- Copy raw bytes
- View node details
- Navigate directly to matching fields

---

## Accuracy Requirements

Correct decoding is more important than visual appearance.

The application must avoid silently guessing when a value cannot be reliably decoded.

If something cannot be decoded, display the problem clearly and preserve the original bytes.

For example:

`Tag 17`
- Offset: `0x000013A4`
- Length: `8`
- Raw: `8F A3 11 00 43 B2 09 FF`
- Status: `Unknown / unsupported encoding`

A malformed or partially unsupported section must not cause the complete file to become unusable.

Decode as much of the file as safely possible.

---

## Parser Design

Keep these responsibilities separate:

1. Binary input reader
2. ASN.1 BER/DER/TLV decoder
3. ASN.1 schema/specification loader
4. Schema-aware semantic decoder
5. TAP-specific interpretation
6. Value formatting/normalization
7. Tree data model
8. User interface

Do not put parsing logic directly inside UI components.

The underlying parser should work independently of the graphical interface so it can also be tested from the command line.

---

## Byte-Level Traceability

Every decoded object should remain traceable to the original file.

Where possible, retain:

- Starting byte offset
- Ending byte offset
- Header bytes
- Tag bytes
- Length bytes
- Value bytes
- Raw hexadecimal representation

This is important for debugging incorrect telecom encodings and validating the viewer against external ASN.1 tools.

A selected tree node should therefore be capable of showing exactly which part of the original binary file produced it.

---

## Performance

The application should remain lightweight and responsive.

Important priorities:

- Fast startup
- Low memory consumption
- Efficient binary parsing
- Avoid unnecessary copies of large byte arrays
- Lazy-load tree branches where beneficial
- Avoid decoding the same section multiple times
- Support reasonably large telecom files
- Keep UI operations responsive while parsing

Do not introduce heavy frameworks unless they provide a clear technical benefit.

---

## User Experience

The application should be minimal and easy to understand.

A typical workflow should be:

1. Open the application.
2. Select or drag-and-drop a binary file.
3. Application detects or asks for the appropriate specification.
4. File is decoded.
5. Results appear as a tree.
6. User expands objects and examines values.
7. User can inspect raw bytes/details when needed.

The main screen should not overwhelm the user with ASN.1 internals.

Technical information should still be available for debugging.

---

## Architecture Philosophy

Prefer established, well-tested ASN.1 libraries where they provide correct behavior.

Do not manually reimplement the entire ASN.1 standard unless necessary.

However, the application must retain enough low-level access to the binary representation to expose:

- tags
- lengths
- values
- raw bytes
- offsets
- decoding errors

The final design should prioritize:

**Correctness > maintainability > performance > visual complexity**

The software should be:

- Lightweight
- Fast
- Minimal
- Accurate
- Modular
- Extensible
- Easy to debug
- User-friendly

---

## Important Development Rule

Before implementing format-specific logic, inspect:

- The provided ASN.1 specification files
- Available sample binary files
- Existing code, if any
- Existing dependencies
- Actual byte structures in sample files

Do not read unrelated directories or unnecessary files.

Use targeted inspection to keep development efficient.

Do not invent ASN.1 field mappings.

Field names and structures must come from:

1. The ASN.1 specification,
2. Valid encoding rules,
3. Or clearly documented format-specific knowledge.

If a field cannot be confidently interpreted, preserve and expose the raw representation rather than returning a misleading value.

---

## Definition of Success

The project is successful when a user can open a supported ASN.1 or TAP binary file and immediately see an accurate hierarchical representation similar to:

`File`
- `Object`
  - `Field`
    - Tag
    - Value
  - `NestedObject`
    - `Field`
      - Value

while still being able to inspect the underlying tag numbers, byte offsets, lengths, raw bytes, and decoding details whenever necessary.

The viewer should make complex telecom ASN.1 files understandable without hiding the underlying binary structure.