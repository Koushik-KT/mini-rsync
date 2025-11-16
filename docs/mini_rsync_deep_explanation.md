# Mini-Rsync Deep Architecture & Interview Notes

This document expands on the PDF and provides talking points for interviews.

## Key components and what to say

- **Mutual TLS (mTLS)**: Explain that mTLS provides both server and client authentication, preventing unauthorized clients from uploading data. Mention certificate chain and CA verification.
- **Resume support**: The client sends an offset and server appends to a `.tmp` file — this allows resuming interrupted transfers.
- **Atomic write**: Server writes to `.tmp` and renames once transfer completes — atomic on same filesystem.
- **Threading**: ThreadPool used in earlier versions to parallelize transfers; in TLS version initial scan is sequential for clarity but can be parallelized.
- **Inotify**: Kernel-level filesystem events allow near real-time detection instead of polling.
- **Integrity**: SHA-256 used to detect content changes (available via util), helpful to avoid unnecessary transfers.
- **Next steps**: chunk checksums, delta-transfer, compression, authenticated metadata, and unit tests.

Use these short paragraphs in interviews; they demonstrate design thinking and security awareness.
