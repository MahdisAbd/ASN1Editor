ASN1Viewer - Portable Windows Distribution
===========================================

Run asn1viewer_gui.exe to open the graphical application.
The application can be copied to another Windows 10/11 x64 computer without
installing Visual Studio, Qt, CMake, or the ASN.1 build tools.

The ASN.1 specification files in this folder are required for schema-aware
decoding. Keep them beside the executable.

Command-line usage:

  asn1viewer.exe decode input-file.dat

For an explicit specification and root type:

  asn1viewer.exe decode input-file.dat --schema HUAWEI_R9.asn --root PGWRecord

The application is built for Windows x64. It requires Windows 10 or newer.
