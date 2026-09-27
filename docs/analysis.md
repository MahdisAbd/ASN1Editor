# Repository Analysis

The repository initially contains seven ASN.1 specifications and no executable
source, build system, binary samples, dependencies, or test suite.

## Available specifications

- `TAP-0312.asn`: TAP release 3.12; application-tagged structures including
  `DataInterChange`, `TransferBatch`, call-event records, IMSI, MSISDN, and
  location data.
- `nrtrde.asn`: NRTRDE module with implicit/default tagging.
- `nrtrde_v2.asn`: NRTRDE 2.1 with application tags.
- `CS_CDR.asn`, `huawei-cdr.asn`, and `HUAWEI_R9.asn`: telecom CDR variants.

Four binary samples are available under `input/`, ranging from approximately
1.0 MB to 1.4 MB. Their leading encodings include context/application-style
high tags and one `SEQUENCE` root. They are suitable for integration and
performance tests, but their exact schema/root association still needs to be
confirmed against expected production output.

## Constraints

The viewer must preserve encoded offsets, lengths, tags, and raw bytes even
when schema resolution fails. Generic BER parsing must remain independent from
TAP-specific interpretation. Runtime schema loading is preferred over generated
code so additional specifications can be added without recompiling the parser.
