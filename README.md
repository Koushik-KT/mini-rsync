# Mini-Rsync TLS — C++ Multithreaded File Sync & Backup Utility (Unix) (TLS + Resumable)

## 🧠 Project Summary

A multithreaded C++ file synchronization and backup utility for Unix-based systems. It monitors directories in real-time, detects file changes using inotify, resumable chunked transfers, computes SHA-256 hashes, and synchronizes modified files to a remote server over TCP and do end-to-end encryption via mutual TLS (mTLS). Built with POSIX APIs, thread pools, and efficient zero-copy file transfer logic.

**🔧 Built using only C++, POSIX system calls, and OpenSSL.**

## 🚀 Features

- 🔐 Secure Transfer

  - Mutual TLS (client & server certificate verification)
  - OpenSSL-based encrypted transport layer
  - Protection against MITM attacks

- ⚡ High-Performance Sync

  - Multithreaded worker pool for parallel file chunk uploads
  - Efficient job queue with lock-guarded task scheduling
  - Chunk-based resume support (append from last offset)

- 📡 Real-Time Monitoring

  - Linux inotify detects:
    - file create
    - modify
    - delete
    - move
  - Immediate upload to server on change

- 🧱 Data Integrity

  - SHA-256 hashing
  - Temporary .tmp files + atomic rename
  - Corruption-safe partial uploads

## 🏗 Architecture Overview

```
flowchart LR
    subgraph Client[Client (Linux)]
        A[Inotify Watcher] --> B[Job Queue]
        B --> C[Thread Pool]
        C -->|Chunked Upload| TLSClient[TLS Socket]
    end

    TLSClient -->|Encrypted Stream| TLSServer[TLS Server]

    subgraph Server[Server]
        TLSServer --> D[Chunk Handler]
        D --> E[Temp .tmp File]
        E --> F[Atomic Rename]
        F --> G[Final Synced File]
    end
```

## Build requirements

- Linux (inotify)
- g++ (C++17)
- libssl-dev (OpenSSL)
- make

Install dependencies (Debian/Ubuntu):

```bash
sudo apt update && sudo apt install -y build-essential libssl-dev
```

## Generate test certificates (self-signed) — quick guide

This project expects PEM files for server and client. For testing only, create a self-signed CA, sign server and client certs.

```bash
# create CA
openssl genrsa -out ca.key 4096
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt -subj "/CN=MiniRsync-CA"

# server key + CSR + cert
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=mini-rsync-server"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 3650 -sha256

# client key + CSR + cert
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr -subj "/CN=mini-rsync-client"
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 3650 -sha256
```

Place `server.crt`, `server.key`, and `ca.crt` next to the server binary. Place `client.crt`, `client.key`, and `ca.crt` next to the client binary.

## 🔧 Build & Run

```bash
make
```

- Start TLS Server
``` ./mini_sync_server_tls ```

- Run Client (watch folder)
``` ./mini_sync_client_tls ./my_folder ```

## 🔐 Certificates (mTLS)

- Generate certificates using: ``` ./docs/generate_certs.sh ```

- The script creates:
  - ca.crt, ca.key
  - server.crt, server.key
  - client.crt, client.key
- Client validates server, server validates client.

## 📘 Technologies Used

- C++17 (modern C++)
- POSIX Sockets
- OpenSSL mTLS
- Linux Inotify
- Thread Pool
- SHA-256 Hashing

## 🧪 What This Project Demonstrates

✔ Systems programming
✔ Concurrency & synchronization
✔ Security engineering (TLS/mTLS)
✔ Linux internals (inotify, epoll, file descriptors)
✔ Network programming
✔ Low-level performance optimization

## Protocol (summary)

1. TLS handshake (mutual): both sides verify peer cert using `ca.crt`.
2. Client sends: 4-byte filename length (network), filename bytes, 8-byte file size (network), 8-byte offset (network).
3. Server responds with 1 byte: 0 = OK to send, 1 = error.
4. Client streams bytes from `offset` onwards in 8KB chunks.
5. Server writes to `<dest>/<filename>.tmp`, fsyncs, and renames to final filename on completion.

## Limitations

- Self-signed certs only for testing.
- No authentication beyond cert verification.
- Not production hardened; uses blocking I/O for clarity.

## 📝 License

- MIT License

## 🏆 This project is ideal for:

- Systems Engineer
- Backend Developer
- DevOps/SRE roles
- Low-level/C++ positions

## Developed by **Koushik Tripathy** ##
