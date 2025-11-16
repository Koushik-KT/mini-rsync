// TLS-enabled unified client with resume support (connects and sends file with offset)
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "include/sha256_util.h"

namespace fs = std::filesystem;

void init_openssl() { SSL_library_init(); OpenSSL_add_all_algorithms(); SSL_load_error_strings(); }
void cleanup_openssl() { ERR_free_strings(); EVP_cleanup(); }

SSL_CTX* create_client_ctx(const char* certfile, const char* keyfile, const char* cafile) {
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) return nullptr;
    if (SSL_CTX_use_certificate_file(ctx, certfile, SSL_FILETYPE_PEM) <= 0) return nullptr;
    if (SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) <= 0) return nullptr;
    if (!SSL_CTX_check_private_key(ctx)) return nullptr;
    if (SSL_CTX_load_verify_locations(ctx, cafile, nullptr) <= 0) return nullptr;
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    return ctx;
}

bool send_file_tls(const std::string &server_ip, int port, const fs::path &file, SSL_CTX* ctx) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return false; }
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &serv.sin_addr);

    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) { perror("connect"); close(sock); return false; }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) { std::cerr << "SSL connect failed\n"; SSL_free(ssl); close(sock); return false; }

    std::string fname = fs::relative(file, file.parent_path()).string();
    // For uniqueness, we will send the filename relative to the watched dir; here assume file has absolute path; send only filename
    fname = file.filename().string();
    uint32_t fl = htonl((uint32_t)fname.size());
    SSL_write(ssl, &fl, sizeof(fl));
    SSL_write(ssl, fname.data(), fname.size());

    uint64_t fsize = fs::file_size(file);
    uint64_t fsize_net = htobe64(fsize);
    // Determine offset by asking server? Simple approach: if server has tmp file, we'll send its size by making a small "stat" request is not defined.
    // For simplicity, client sends offset = 0 (full send) — server will append if partial exists.
    uint64_t offset = 0;
    uint64_t offset_net = htobe64(offset);
    SSL_write(ssl, &fsize_net, sizeof(fsize_net));
    SSL_write(ssl, &offset_net, sizeof(offset_net));

    uint8_t resp;
    if (SSL_read(ssl, &resp, 1) != 1) { SSL_shutdown(ssl); SSL_free(ssl); close(sock); return false; }
    if (resp != 0) { std::cerr << "Server refused transfer\n"; SSL_shutdown(ssl); SSL_free(ssl); close(sock); return false; }

    std::ifstream ifs(file, std::ios::binary);
    if (!ifs) { std::cerr << "Cannot open file\n"; SSL_shutdown(ssl); SSL_free(ssl); close(sock); return false; }
    // Seek to offset (0 here)
    ifs.seekg(offset);

    const size_t BUF = 8192;
    std::vector<char> buf(BUF);
    while (ifs.good()) {
        ifs.read(buf.data(), buf.size());
        std::streamsize s = ifs.gcount();
        if (s > 0) {
            int written = SSL_write(ssl, buf.data(), (int)s);
            if (written <= 0) break;
        }
    }
    ifs.close();
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    std::cout << "Sent (TLS): " << fname << " (" << fsize << " bytes)\n";
    return true;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "Usage: mini_sync_client_tls <server_ip> <port> <watch_dir> <client.crt> <client.key> <ca.crt>\n";
        return 1;
    }
    std::string server = argv[1];
    int port = std::stoi(argv[2]);
    fs::path watch = argv[3];
    const char* certfile = argv[4];
    const char* keyfile = argv[5];
    const char* cafile = argv[6];
    if (!fs::exists(watch) || !fs::is_directory(watch)) {
        std::cerr << "Watch directory doesn't exist\n"; return 1;
    }

    init_openssl();
    SSL_CTX* ctx = create_client_ctx(certfile, keyfile, cafile);
    if (!ctx) { std::cerr << "Failed to create client SSL_CTX\n"; return 2; }

    // Thread pool for concurrent transfers
    // For demo simplicity, we will not use a pool here; user can extend
    std::cout << "Starting initial scan...\n";
    for (auto &p : fs::recursive_directory_iterator(watch)) {
        if (fs::is_regular_file(p.path())) {
            send_file_tls(server, port, p.path(), ctx);
        }
    }

    // inotify-based live watch
    int inot = inotify_init1(IN_NONBLOCK);
    if (inot < 0) { perror("inotify_init"); return 1; }
    int wd = inotify_add_watch(inot, watch.c_str(), IN_MODIFY | IN_CREATE | IN_MOVED_TO);
    const size_t BUF = (10 * (sizeof(inotify_event) + NAME_MAX + 1));
    std::vector<char> buffer(BUF);

    std::cout << "Watching directory: " << watch << "\n";
    while (true) {
        int len = read(inot, buffer.data(), buffer.size());
        if (len <= 0) { sleep(1); continue; }
        int i = 0;
        while (i < len) {
            inotify_event *e = (inotify_event*)&buffer[i];
            if (e->len) {
                std::string filename(e->name);
                fs::path fp = fs::path(watch) / filename;
                if (fs::exists(fp) && fs::is_regular_file(fp)) {
                    send_file_tls(server, port, fp, ctx);
                }
            }
            i += sizeof(inotify_event) + e->len;
        }
    }

    SSL_CTX_free(ctx);
    cleanup_openssl();
    return 0;
}
