CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -pthread
LDFLAGS = -lssl -lcrypto

SRC_SERVER = src/sync_server_tls.cpp src/threadpool.cpp src/sha256_util.cpp
SRC_CLIENT = src/sync_client_tls.cpp src/watcher_tls.cpp src/threadpool.cpp src/sha256_util.cpp

.PHONY: all clean

all: bin/mini_sync_server_tls bin/mini_sync_client_tls

bin/mini_sync_server_tls: $(SRC_SERVER)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

bin/mini_sync_client_tls: $(SRC_CLIENT)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf bin server_storage *.srl
