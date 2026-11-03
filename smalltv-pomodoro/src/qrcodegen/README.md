# Nayuki QR Code generator

Vendored from Project Nayuki, version 1.8.0:
https://github.com/nayuki/QR-Code-generator/tree/v1.8.0/c

MIT license, retained in both source files and licenses/qrcodegen-MIT.txt.
The upstream qrcodegen.c is named qrcodegen.cpp so Arduino and the simulator
compile the same implementation as C++. Trailing whitespace is trimmed; no algorithm changes.
The caller limits codes to version 4 with medium error correction or better.
