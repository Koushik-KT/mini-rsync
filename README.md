# Mini-Rsync TLS — C++ Multithreaded File Sync & Backup Utility (Unix)

## 🧠 Project Summary

"A multithreaded C++ file synchronization and backup utility for Unix-based systems. It monitors directories in real-time, detects file changes using inotify, computes SHA-256 hashes, and synchronizes modified files to a remote server over TCP. Built with POSIX APIs, thread pools, and efficient zero-copy file transfer logic."

## Project Description

This project demonstrates:

- **Mutual TLS (mTLS)** using OpenSSL: server requires client certificate and client verifies server certificate.
- **Unified client** with inotify-based live watching and initial scan.
- **File system handling (stat, inotify, permissions)**
- **Chunked transfer with resume**: client can resume partial uploads by sending an offset.
- **Improved logging and safe write (tmp + rename)**
- **Multithreading (C++ threads / mutex / condition variables)**
- **Low-level Unix system calls**
- **Socket programming (TCP client/server)**
- **Hashing algorithms (SHA-256 for integrity)**
- **Real-world functionality**
- **Clean architecture**
It contains **Makefile** with build targets for server & client.

## 🚀 How the Project Works

1. Directory is monitored using inotify

- inotify_init(), inotify_add_watch()
- These detect:
  - File created
  - File modified
  - File deleted
  - File renamed
- Every event is pushed into a work queue.

**Example**:
"test.txt was modified" → add to queue

2. A file-change event triggers a hash comparison

- The system computes a SHA-256 hash:
  - If old hash == new hash → do nothing
  - If different → file is added to sync queue
- This avoids sending unchanged files.

3. Thread Pool Handles Sync Jobs

- To speed things up:
  - Several worker threads wait on a condition variable
  - When a job arrives, a free thread processes it
  - This uses std::thread, std::mutex, std::condition_variable

4. File Sent Over TCP Socket

- The client connects to the remote server:

```
connect(socket, serverAddress)
send(fileName)
send(fileSize)
send(fileDataChunks)
```

- On the server side:

```
accept()
read header
create file
write received data
fsync() to ensure disk safety
```

- This teaches real Unix socket programming.

5. Server Writes File to Destination Directory

- Uses open(), write(), close()
- Permissions set using chmod()
- Ensures partial transfers don’t corrupt files
- Temporary file → renamed atomically using rename()

6. Logs Are Written

- Both sides log:
  - Time of transfer
  - File size
  - Number of chunks
  - Thread ID

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

## Build

```bash
make
```

## Run (example)

Start server (listens and requires client cert):

```bash
mkdir -p server_storage
./bin/mini_sync_server_tls 9443 server_storage server.crt server.key ca.crt
```

Run client (presents client cert):

```bash
./bin/mini_sync_client_tls 127.0.0.1 9443 /path/to/watch client.crt client.key ca.crt
```

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

## Developed by **Koushik Tripathy** ##
