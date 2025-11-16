// TLS-enabled file receive server (mutual TLS required)
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

namespace fs = std::filesystem;

void init_openssl() { SSL_library_init(); OpenSSL_add_all_algorithms(); SSL_load_error_strings(); }
void cleanup_openssl() { ERR_free_strings(); EVP_cleanup(); }

SSL_CTX* create_server_ctx(const char* certfile, const char* keyfile, const char* cafile) {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) return nullptr;
    if (SSL_CTX_use_certificate_file(ctx, certfile, SSL_FILETYPE_PEM) <= 0) return nullptr;
    if (SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) <= 0) return nullptr;
    if (!SSL_CTX_check_private_key(ctx)) return nullptr;
    // Require client certificate and verify against CA
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    if (SSL_CTX_load_verify_locations(ctx, cafile, nullptr) <= 0) return nullptr;
    return ctx;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: mini_sync_server_tls <port> <dest_dir> <server.crt> <server.key> <ca.crt>\n";
        return 1;
    }
    int port = std::stoi(argv[1]);
    fs::path dest = argv[2];
    const char* certfile = argv[3];
    const char* keyfile = argv[4];
    const char* cafile = argv[5];
    fs::create_directories(dest);

    init_openssl();
    SSL_CTX* ctx = create_server_ctx(certfile, keyfile, cafile);
    if (!ctx) { std::cerr << "Failed to create SSL_CTX\n"; return 2; }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 10) < 0) { perror("listen"); return 1; }

    std::cout << "TLS Server listening on port " << port << "\n";

    while (true) {
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        int client_fd = accept(server_fd, (sockaddr*)&client, &len);
        if (client_fd < 0) { perror("accept"); continue; }
        std::cout << "Incoming connection...\n";

        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);
        if (SSL_accept(ssl) <= 0) {
            std::cerr << "TLS handshake failed\n";
            SSL_free(ssl);
            close(client_fd);
            continue;
        }
        // Verify peer cert present (already enforced by ctx), but we can print subject.
        X509* cert = SSL_get_peer_certificate(ssl);
        if (cert) {
            char *subj = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
            std::cout << "Client cert subject: " << (subj ? subj : "(nil)") << "\n";
            if (subj) OPENSSL_free(subj);
            X509_free(cert);
        } else {
            std::cout << "No client cert presented\n";
        }

        // Read filename length
        uint32_t fl_net;
        if (SSL_read(ssl, &fl_net, sizeof(fl_net)) != sizeof(fl_net)) { SSL_shutdown(ssl); SSL_free(ssl); close(client_fd); continue; }
        uint32_t fl = ntohl(fl_net);
        std::string fname(fl, '\\0');
        if (SSL_read(ssl, fname.data(), fl) != (int)fl) { SSL_shutdown(ssl); SSL_free(ssl); close(client_fd); continue; }

        uint64_t fsize_net, offset_net;
        if (SSL_read(ssl, &fsize_net, sizeof(fsize_net)) != sizeof(fsize_net)) { SSL_shutdown(ssl); SSL_free(ssl); close(client_fd); continue; }
        if (SSL_read(ssl, &offset_net, sizeof(offset_net)) != sizeof(offset_net)) { SSL_shutdown(ssl); SSL_free(ssl); close(client_fd); continue; }
        uint64_t fsize = be64toh(fsize_net);
        uint64_t offset = be64toh(offset_net);

        fs::path tmp = dest / (fname + ".tmp");
        fs::path final = dest / fname;
        std::ofstream ofs(tmp, std::ios::binary | std::ios::app);
        if (!ofs) {
            uint8_t err = 1;
            SSL_write(ssl, &err, 1);
            SSL_shutdown(ssl); SSL_free(ssl); close(client_fd); continue;
        }
        // Seek to offset if file already has data
        ofs.seekp(offset);
        uint64_t remaining = fsize > offset ? (fsize - offset) : 0;
        uint64_t written = 0;
        const size_t BUF = 8192;
        std::vector<char> buf(BUF);
        uint8_t ok = 0;
        SSL_write(ssl, &ok, 1); // OK to send
        while (remaining > 0) {
            int toread = (int)std::min<uint64_t>(BUF, remaining);
            int r = SSL_read(ssl, buf.data(), toread);
            if (r <= 0) break;
            ofs.write(buf.data(), r);
            remaining -= r;
            written += r;
        }
        ofs.flush();
        ofs.close();
        // fsync - write changes to disk: use rename to final if sizes match
        if (fs::exists(tmp) && fs::file_size(tmp) == fsize) {
            fs::rename(tmp, final);
            std::cout << "Received and stored: " << final << " (" << fsize << " bytes)\n";
        } else {
            std::cout << "Partial received for " << fname << " (" << written << " bytes)\n";
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_fd);
    }

    SSL_CTX_free(ctx);
    cleanup_openssl();
    close(server_fd);
    return 0;
}
