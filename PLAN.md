# ASN.1 Viewer — Implementation Plan

## 1. Objective

Build a lightweight, fast, and user-friendly desktop application for opening and inspecting:

- Generic ASN.1 binary files
- TAP telecom files
- Other telecom binary files encoded using ASN.1

The application must decode binary data accurately and present it as a clean, human-readable hierarchical tree.

The viewer must preserve enough low-level information to verify every decoded value against the original binary file.

Primary priorities:

1. Correct decoding
2. Clear architecture
3. Reliable schema support
4. Good performance
5. Simple user experience

---

# 2. Development Principles

During implementation:

- Inspect only relevant files.
- Avoid scanning large unrelated directories.
- Reuse established ASN.1 libraries where practical.
- Do not manually reimplement ASN.1 unless necessary.
- Keep parsing logic independent from the UI.
- Preserve raw bytes for every decoded object.
- Never invent field names or values.
- Prefer explicit decoding failures over incorrect guesses.
- Add tests for every decoding behavior introduced.
- Keep dependencies minimal.
- Keep the application lightweight.

---

# 3. Target Architecture

The application should be divided into the following layers:

```text
Binary File
    |
    v
Binary Reader
    |
    v
ASN.1 TLV Decoder
    |
    +----------------------+
    |                      |
    v                      v
Generic ASN.1 Tree     Schema Resolver
                           |
                           v
                    Semantic Decoder
                           |
                     +-----+------+
                     |            |
                     v            v
                 Generic       TAP Decoder
                 ASN.1
                     \            /
                      \          /
                       v        v
                       Tree Model
                           |
                           v
                      User Interface
```

Major components:

```text
src/
├── core/
│   ├── binary/
│   ├── asn1/
│   ├── schema/
│   ├── decoder/
│   └── model/
│
├── formats/
│   └── tap/
│
├── ui/
│
├── tests/
│
├── samples/
│
└── specifications/
```

The exact structure may change depending on the selected language/framework.

---

# 4. Phase 0 — Repository and Input Analysis

## Goal

Understand the available input data before making architectural decisions.

## Tasks

Inspect:

- Existing source code
- Existing dependencies
- ASN.1 specification files
- TAP specifications
- Sample TAP files
- Other ASN.1 samples
- Existing expected outputs, if available

Determine:

- ASN.1 encoding rules used
- BER / DER / CER usage
- Common telecom-specific encodings
- File sizes
- Schema structure
- TAP ASN.1 structure
- Existing parsing limitations

Do not recursively read unrelated directories.

Use targeted inspection.

## Deliverable

Create:

```text
docs/analysis.md
```

Document:

- Current repository state
- Available sample formats
- Available ASN.1 specifications
- Observed encodings
- Important technical constraints
- Unknowns
- Recommended architecture

## Success Criteria

Phase 0 is complete when:

- Relevant specifications are identified.
- Sample files are identified.
- Encoding types are understood.
- Existing implementation is understood.
- Major parser requirements are documented.

---

# 5. Phase 1 — Technology Selection

## Goal

Select the best implementation language, ASN.1 library, and UI framework.

## Evaluate

Possible implementation choices may include:

### Java

Advantages:

- Mature ASN.1 libraries
- Existing telecom ecosystem
- Good desktop UI support
- Strong binary processing

Possible libraries:

- ASN1bean
- Bouncy Castle
- jASN1
- Other appropriate ASN.1 libraries

### Python

Advantages:

- Fast development
- Strong ASN.1 ecosystem
- Easy binary analysis

Possible libraries:

- pyasn1
- asn1tools
- construct
- other suitable libraries

Possible UI frameworks:

- PySide6
- PyQt
- lightweight alternatives

### Other Languages

Consider Rust, C#, or another language only if they offer clear advantages.

## Evaluation Criteria

Compare:

- ASN.1 decoding support
- Schema support
- BER support
- TAP compatibility
- Desktop packaging
- Performance
- Memory use
- Maintainability
- UI quality
- Dependency weight

## Deliverable

Create:

```text
docs/technology-decision.md
```

Record:

- Selected language
- Selected ASN.1 library
- Selected UI framework
- Reasons
- Rejected alternatives

## Success Criteria

Technology choices are finalized before major implementation begins.

---

# 6. Phase 2 — Binary Reader

## Goal

Implement a reliable low-level binary file reader.

## Requirements

Support:

- Sequential reading
- Random access
- Byte offsets
- Bounded reads
- Hex representation
- Efficient access to large files

The reader should expose:

```text
offset
length
raw bytes
```

Avoid unnecessary copies of large byte arrays.

Consider memory-mapped file access if useful.

## Tests

Test:

- Empty files
- Small files
- Large files
- Boundary conditions
- Invalid offsets
- Partial reads

## Success Criteria

Any byte range in a file can be accurately retrieved and traced by offset.

---

# 7. Phase 3 — Generic ASN.1 TLV Decoder

## Goal

Decode ASN.1 structures without requiring a schema.

## Support

At minimum:

- Universal tags
- Application tags
- Context-specific tags
- Private tags
- Primitive values
- Constructed values
- Short-form length
- Long-form length
- Nested structures
- High tag numbers

Each decoded node must contain:

```text
tag class
tag number
constructed flag
offset
header length
content length
raw bytes
decoded value
children
status
```

## Example Internal Model

```text
ASN1Node
├── offset
├── tagClass
├── tagNumber
├── constructed
├── length
├── rawBytes
├── decodedValue
├── children
└── error
```

## Error Handling

Do not terminate the entire decode when encountering malformed data.

Return something similar to:

```text
status: malformed
offset: 0x013A
raw: ...
reason: invalid length
```

Continue decoding when safe.

## Success Criteria

Generic BER-style ASN.1 data can be displayed correctly without a schema.

---

# 8. Phase 4 — Primitive Value Decoding

## Goal

Provide readable representations of ASN.1 primitive types.

## Implement

Support common types:

- BOOLEAN
- INTEGER
- ENUMERATED
- NULL
- OCTET STRING
- BIT STRING
- OBJECT IDENTIFIER
- UTF8String
- PrintableString
- IA5String
- NumericString
- VisibleString
- GeneralString
- UTCTime
- GeneralizedTime

Where appropriate show:

```text
Value: 512
Raw: 00 02 00
```

Do not destroy the original representation.

## Success Criteria

Supported ASN.1 primitive values are displayed correctly and consistently.

---

# 9. Phase 5 — Generic Tree Model

## Goal

Create a UI-independent model representing decoded files.

## Tree Node Fields

A node should be capable of containing:

```text
displayName
fieldName
tagClass
tagNumber
type
offset
length
value
rawValue
children
decodeStatus
schemaReference
```

The UI must consume this model rather than directly accessing parser internals.

## Success Criteria

The same parsed tree can be:

- displayed in the UI
- exported
- tested
- inspected from CLI tools

without depending on UI code.

---

# 10. Phase 6 — ASN.1 Specification Loader

## Goal

Load supplied ASN.1 specifications.

## Capabilities

Parse or compile specifications and extract:

- Type definitions
- Field names
- Tag numbers
- Constraints
- ENUM values
- SEQUENCE
- SET
- CHOICE
- SEQUENCE OF
- SET OF
- Explicit tags
- Implicit tags

Support multiple ASN.1 modules.

Handle:

```text
IMPORTS
EXPORTS
```

where practical.

## Important Rule

Do not hardcode field names that already exist in specifications.

## Success Criteria

The viewer can map schema definitions to binary structures.

---

# 11. Phase 7 — Schema-Aware Decoder

## Goal

Combine binary parsing with ASN.1 specification information.

Convert generic structures such as:

```text
Context Tag 3
  Context Tag 1
  Context Tag 4
```

into something closer to:

```text
MobileOriginatedCall
  CallingNumber
  CalledNumber
  LocationInformation
```

while preserving technical information.

## Node Example

```text
LocationAreaCode
Tag: [3]
Offset: 0x14A2
Length: 2
Value: 12345
Raw: 30 39
```

## Success Criteria

Known schema fields display their real names and values.

---

# 12. Phase 8 — TAP Support

## Goal

Implement TAP-specific interpretation.

TAP is a priority format and should receive additional schema-aware presentation.

## Requirements

Support:

- TAP root objects
- Transfer batches
- Call/event records
- Nested telecom objects
- TAP-specific data types
- Known telecom encodings

Presentation should prioritize meaningful names.

Example:

```text
TransferBatch
├── BatchControlInfo
│   ├── Sender
│   ├── Recipient
│   └── FileCreationTimestamp
│
└── CallEventDetails
    ├── MobileOriginatedCall
    │   ├── CallingNumber
    │   ├── CalledNumber
    │   ├── LocationInformation
    │   │   ├── LocationAreaCode
    │   │   └── CellId
    │   └── CallDuration
```

Do not mix TAP-specific rules into the generic decoder.

Place TAP logic under something similar to:

```text
formats/tap/
```

## Success Criteria

Provided TAP samples decode to meaningful named structures.

---

# 13. Phase 9 — Telecom Value Formatters

## Goal

Make known telecom values human-readable.

Potential fields include:

- IMSI
- IMEI
- MSISDN
- PLMN
- MCC
- MNC
- LAC
- Cell ID
- Timestamps
- TBCD values
- BCD values
- Addresses
- Call durations
- Charging fields

Each formatter must be:

- isolated
- testable
- specification-driven where possible

## Important

Always preserve raw values.

Example:

```text
IMSI
Value: 432110123456789
Raw: 34 12 01 21 43 65 87 F9
Encoding: TBCD
```

## Success Criteria

Known telecom values are displayed correctly without losing raw representation.

---

# 14. Phase 10 — Minimal Desktop UI

## Goal

Build the initial graphical viewer.

## Main Screen

Suggested layout:

```text
+----------------------------------------------------+
| File | Open | Search | Expand All | Collapse All   |
+----------------------------------------------------+
|                                                    |
| ASN.1 Tree                                        |
|                                                    |
|  > TransferBatch                                  |
|      > BatchControlInfo                           |
|      > CallEventDetails                           |
|                                                    |
+----------------------------------------------------+
| Selected Node Details                              |
| Tag:                                               |
| Offset:                                            |
| Length:                                            |
| Value:                                             |
| Raw:                                               |
+----------------------------------------------------+
```

## Required Features

Implement:

- Open file
- Drag and drop
- Tree navigation
- Expand node
- Collapse node
- Expand all
- Collapse all
- Node details panel
- Copy value
- Copy raw bytes

## Success Criteria

A user can open a supported file and navigate its decoded structure visually.

---

# 15. Phase 11 — Search and Navigation

## Goal

Make large telecom files practical to inspect.

## Features

Search by:

- Field name
- Tag number
- Value
- Raw hex value
- Object type

Support:

```text
Next result
Previous result
```

Optional filters:

```text
Only errors
Only primitive fields
Only matching objects
```

## Success Criteria

Users can quickly locate fields inside large ASN.1 files.

---

# 16. Phase 12 — Raw Hex View

## Goal

Allow binary-level verification.

When a user selects a tree node, optionally show:

```text
Offset
Tag bytes
Length bytes
Value bytes
Full encoded object
```

Example:

```text
Offset: 0x000013A4

A3 08
   81 02 30 39
   82 02 01 7F
```

Highlighting the selected object's byte range is desirable.

## Success Criteria

Every displayed node can be traced back to its original bytes.

---

# 17. Phase 13 — Large File Performance

## Goal

Keep the application responsive with large telecom files.

## Optimizations

Investigate:

- Lazy tree expansion
- Incremental decoding
- Background parsing
- Memory mapping
- Cached decoded nodes
- Deferred raw-byte conversion
- Avoiding duplicate buffers

Measure:

- Startup time
- File open time
- Parsing time
- Memory usage
- Tree rendering time

## Success Criteria

Large files can be opened without freezing the application or consuming unreasonable memory.

---

# 18. Phase 14 — Error Reporting

## Goal

Make decoding failures useful for debugging.

Errors should include:

```text
Offset
Tag
Raw bytes
Error reason
Parent object
Expected type
Actual encoding
```

Example:

```text
Decode Error

Field:
LocationAreaCode

Offset:
0x000014FA

Expected:
OCTET STRING length 2

Received:
OCTET STRING length 3

Raw:
81 03 12 34 56
```

## Success Criteria

A malformed field can be diagnosed without manually tracing the entire binary file.

---

# 19. Phase 15 — CLI Decoder

## Goal

Provide a non-UI way to test the decoder.

Example:

```bash
asn1viewer decode sample.tap
```

Optional:

```bash
asn1viewer decode sample.tap --schema tap.asn
```

Output:

```text
TransferBatch
  BatchControlInfo
  CallEventDetails
    MobileOriginatedCall
      LocationAreaCode = 12345
      CellId = 4231
```

Possible export:

```bash
asn1viewer decode sample.tap --json output.json
```

## Purpose

The CLI makes parser development easier and allows testing independent of the UI.

## Success Criteria

Decoder behavior can be validated without launching the GUI.

---

# 20. Phase 16 — Test Suite

## Goal

Create strong automated validation.

## Unit Tests

Test:

- Tag decoding
- Length decoding
- Primitive values
- Nested structures
- Invalid data
- ASN.1 schema loading
- ENUM decoding
- Telecom value conversion
- TAP structures

## Regression Tests

For each important sample file:

```text
input file
+
ASN.1 specification
+
expected decoded subset
```

Validate fields such as:

```text
LocationAreaCode
CellId
IMSI
MSISDN
timestamps
record counts
```

## Golden Tests

Where practical, compare against:

- Known-good decoder
- Existing production output
- Expected JSON representation

## Success Criteria

Important decoding behavior is protected from regressions.

---

# 21. Phase 17 — Validation Against External Tools

## Goal

Verify correctness independently.

For selected files compare:

```text
ASN.1 Viewer output
vs.
trusted ASN.1 decoder
vs.
specification
```

Verify:

- Tag numbers
- Lengths
- Byte offsets
- Object boundaries
- Decoded integers
- Telecom values
- Named fields

## Success Criteria

Critical fields match trusted external decoders and specifications.

---

# 22. Phase 18 — Export Features

## Goal

Allow users to save decoded information.

Potential export formats:

```text
JSON
TXT
CSV
XML
```

JSON is highest priority.

Example:

```json
{
  "type": "LocationInformation",
  "offset": 4830,
  "children": {
    "locationAreaCode": 12345,
    "cellId": 4501
  }
}
```

Raw metadata may optionally be included.

## Success Criteria

Decoded files can be exported without losing structural hierarchy.

---

# 23. Phase 19 — Packaging

## Goal

Produce an easy-to-run application.

Desired result:

Windows:

```text
ASN1Viewer.exe
```

or equivalent packaged application.

Avoid requiring users to manually install development dependencies.

## Validate

- Clean installation
- Startup
- File opening
- Specification loading
- Large file handling
- Error handling

## Success Criteria

The viewer can run on a clean target machine.

---

# 24. Phase 20 — Documentation

Create:

```text
README.md
docs/
├── architecture.md
├── supported-formats.md
├── adding-specifications.md
├── decoder-design.md
├── telecom-formatters.md
└── troubleshooting.md
```

README should explain:

```text
What the application does
How to run it
How to open files
How specifications are discovered
How TAP decoding works
How to report decoding problems
```

---

# 25. Suggested Implementation Order

Use this order:

```text
Phase 0
Repository analysis

Phase 1
Technology selection

Phase 2
Binary reader

Phase 3
Generic ASN.1 TLV decoder

Phase 4
Primitive decoding

Phase 5
Tree model

Phase 6
ASN.1 specification loader

Phase 7
Schema-aware decoder

Phase 8
TAP support

Phase 9
Telecom value formatters

Phase 10
Minimal UI

Phase 11
Search/navigation

Phase 12
Raw hex view

Phase 13
Performance optimization

Phase 14
Error diagnostics

Phase 15
CLI

Phase 16
Automated tests

Phase 17
External validation

Phase 18
Export

Phase 19
Packaging

Phase 20
Documentation
```

Do not start with the UI.

Correct decoding must exist before significant UI work.

---

# 26. MVP Scope

The first usable version should include only:

```text
1. Open binary file
2. Generic BER/TLV decoding
3. Tree representation
4. Tag number
5. Tag class
6. Length
7. Decoded primitive value
8. Raw bytes
9. Byte offset
10. Load ASN.1 schema
11. Show schema field names
12. Basic TAP decoding
13. Search
14. Node details
```

Avoid unnecessary features before this works correctly.

---

# 27. Non-Goals for Initial Versions

Do not prioritize:

- Full ASN.1 editor
- Binary modification
- Binary re-encoding
- Network capture
- Packet sniffing
- Database storage
- Cloud services
- Authentication
- Multi-user features
- Complex themes
- Plugin marketplace

The application is primarily a binary inspection and decoding tool.

---

# 28. Key Risks

## Incorrect Schema Mapping

Risk:

Binary tags may be mapped to the wrong ASN.1 field.

Mitigation:

- Preserve tag numbers.
- Preserve raw bytes.
- Validate against specifications.
- Add regression tests.

---

## Telecom-Specific Encoding

Risk:

Fields such as IMSI or Cell ID may require special encoding logic.

Mitigation:

Create isolated telecom formatters with unit tests.

---

## Large TAP Files

Risk:

Rendering thousands of records could consume large amounts of memory.

Mitigation:

Use lazy tree population and avoid rendering all nodes immediately.

---

## Multiple ASN.1 Specifications

Risk:

Different vendors or versions may use different schemas.

Mitigation:

Keep schema handling modular and allow explicit specification selection.

---

## Decoder Guessing

Risk:

Incorrect automatic interpretation could display misleading values.

Mitigation:

When uncertain, show:

```text
Unknown
```

along with:

```text
raw bytes
tag
offset
length
```

instead of guessing.

---

# 29. Definition of Done

The project is considered production-ready when:

- Generic ASN.1 files decode correctly.
- TAP samples decode correctly.
- Supplied specifications are supported.
- Field names map correctly.
- Raw binary offsets are preserved.
- Telecom values are correctly interpreted.
- Unknown fields remain inspectable.
- Malformed files do not crash the application.
- Large files remain usable.
- Search works.
- Automated tests cover critical decoding.
- Results have been compared with trusted external decoding tools.
- Application can be packaged and launched easily.

---

# 30. Final Success Scenario

A user should be able to open a TAP file and see:

```text
TransferBatch
│
├── BatchControlInfo
│   ├── Sender = ...
│   ├── Recipient = ...
│   └── FileCreationTimestamp = ...
│
└── CallEventDetails
    │
    ├── [0] MobileOriginatedCall
    │   ├── IMSI = ...
    │   ├── MSISDN = ...
    │   ├── LocationInformation
    │   │   ├── LocationAreaCode = 12345
    │   │   └── CellId = 4567
    │   └── CallDuration = ...
    │
    └── [1] MobileTerminatedCall
        └── ...
```

Selecting:

```text
CellId = 4567
```

should allow the user to inspect:

```text
Field: CellId
ASN.1 Type: INTEGER
Tag: [4]
Offset: 0x0000A13F
Length: 2

Decoded:
4567

Raw:
11 D7
```

This combination of:

```text
semantic field name
+
decoded value
+
ASN.1 metadata
+
raw bytes
+
byte offset
```

is the core functionality of the ASN.1 Viewer.