#include "include/sha256_util.h"
#include <openssl/sha.h>
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>

std::string sha256_of_file(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "";

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::vector<char> buf(4096);
    while (ifs.good()) {
        ifs.read(buf.data(), buf.size());
        std::streamsize s = ifs.gcount();
        if (s > 0) SHA256_Update(&ctx, buf.data(), s);
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}
