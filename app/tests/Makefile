# Тесты чистой логики синка (без сборки всего приложения).
#
#   make -C app/tests run
#
# Покрыто:
#   sync_validation_test — пути/имена от пира (path traversal);
#   sync_http_test       — разбор HTTP (неполные заголовки, body, лимиты).

CXX      ?= g++
CC       ?= cc
CXXFLAGS ?= -std=c++17 -fPIC -Wall -Wextra -O2
CFLAGS   ?= -O2 -fPIC -Wall -Wextra

QT_CFLAGS := $(shell pkg-config --cflags Qt5Core)
QT_LIBS   := $(shell pkg-config --libs Qt5Core)

SYNC_DIR := ../src/libraries/sync
MONOCYPHER_DIR := ../../thirdParty/monocypher

all: sync_validation_test sync_http_test sync_tombstones_test sync_diff_test sync_counts_test sync_scope_test sync_preview_test sync_pair_crypto_test sync_pairing_test sync_data_crypto_test

sync_validation_test: sync_validation_test.cpp $(SYNC_DIR)/SyncValidation.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) $< -o $@ $(QT_LIBS)

sync_http_test: sync_http_test.cpp $(SYNC_DIR)/SyncHttp.cpp $(SYNC_DIR)/SyncHttp.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) \
		sync_http_test.cpp $(SYNC_DIR)/SyncHttp.cpp -o $@ $(QT_LIBS)

sync_tombstones_test: sync_tombstones_test.cpp $(SYNC_DIR)/SyncTombstones.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) $< -o $@ $(QT_LIBS)

sync_diff_test: sync_diff_test.cpp $(SYNC_DIR)/SyncDiff.cpp $(SYNC_DIR)/SyncDiff.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) \
		sync_diff_test.cpp $(SYNC_DIR)/SyncDiff.cpp -o $@ $(QT_LIBS)

sync_counts_test: sync_counts_test.cpp $(SYNC_DIR)/SyncCounts.cpp $(SYNC_DIR)/SyncCounts.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) \
		sync_counts_test.cpp $(SYNC_DIR)/SyncCounts.cpp -o $@ $(QT_LIBS)

sync_scope_test: sync_scope_test.cpp $(SYNC_DIR)/SyncScope.cpp $(SYNC_DIR)/SyncScope.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) \
		sync_scope_test.cpp $(SYNC_DIR)/SyncScope.cpp -o $@ $(QT_LIBS)

sync_preview_test: sync_preview_test.cpp $(SYNC_DIR)/SyncPreview.cpp $(SYNC_DIR)/SyncPreview.h
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) \
		sync_preview_test.cpp $(SYNC_DIR)/SyncPreview.cpp -o $@ $(QT_LIBS)

monocypher_test.o: $(MONOCYPHER_DIR)/monocypher.c $(MONOCYPHER_DIR)/monocypher.h
	$(CC) $(CFLAGS) -I$(MONOCYPHER_DIR) -c $(MONOCYPHER_DIR)/monocypher.c -o $@

sync_pair_crypto_test: sync_pair_crypto_test.cpp $(SYNC_DIR)/SyncPairCrypto.cpp $(SYNC_DIR)/SyncPairCrypto.h monocypher_test.o
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) -I$(MONOCYPHER_DIR) \
		sync_pair_crypto_test.cpp $(SYNC_DIR)/SyncPairCrypto.cpp monocypher_test.o -o $@ $(QT_LIBS)

sync_pairing_test: sync_pairing_test.cpp $(SYNC_DIR)/SyncPairing.cpp $(SYNC_DIR)/SyncPairing.h $(SYNC_DIR)/SyncPairCrypto.cpp $(SYNC_DIR)/SyncPairCrypto.h monocypher_test.o
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -I$(SYNC_DIR) -I$(MONOCYPHER_DIR) \
		sync_pairing_test.cpp $(SYNC_DIR)/SyncPairing.cpp $(SYNC_DIR)/SyncPairCrypto.cpp monocypher_test.o -o $@ $(QT_LIBS)

sync_data_crypto_test: sync_data_crypto_test.cpp $(SYNC_DIR)/SyncDataCrypto.cpp $(SYNC_DIR)/SyncDataCrypto.h $(SYNC_DIR)/SyncPairCrypto.cpp $(SYNC_DIR)/SyncPairCrypto.h monocypher_test.o
	$(CXX) $(CXXFLAGS) -ffunction-sections -fdata-sections $(QT_CFLAGS) -I../src -I$(SYNC_DIR) -I$(MONOCYPHER_DIR) \
		sync_data_crypto_test.cpp $(SYNC_DIR)/SyncDataCrypto.cpp $(SYNC_DIR)/SyncPairCrypto.cpp monocypher_test.o -o $@ -Wl,--gc-sections $(QT_LIBS)

run: all
	./sync_validation_test
	./sync_http_test
	./sync_tombstones_test
	./sync_diff_test
	./sync_counts_test
	./sync_scope_test
	./sync_preview_test
	./sync_pair_crypto_test
	./sync_pairing_test
	./sync_data_crypto_test

clean:
	rm -f sync_validation_test sync_http_test sync_tombstones_test sync_diff_test \
	      sync_counts_test sync_scope_test sync_preview_test sync_pair_crypto_test sync_pairing_test sync_data_crypto_test monocypher_test.o

.PHONY: all run clean
