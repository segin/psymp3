/*
 * test_lastfm_protocol_hashing.cpp - Property-based tests for Last.fm protocol hashing (MD5)
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <random>
#include <openssl/evp.h>

// Reference implementation for verification
std::string md5Hash_reference(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) &&
        EVP_DigestUpdate(ctx, input.c_str(), input.length()) &&
        EVP_DigestFinal_ex(ctx, hash, &hash_len)) {
        std::ostringstream hexHash;
        for (unsigned int i = 0; i < hash_len; i++) {
            hexHash << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(hash[i]);
        }
        EVP_MD_CTX_free(ctx);
        return hexHash.str();
    }
    EVP_MD_CTX_free(ctx);
    return "";
}

// Reproduction of the logic in LastFM.cpp for verification
// Note: This matches the production implementation exactly.
std::string protocolMD5_reproduction(const std::string& input) {
    static constexpr char hex_chars[] = "0123456789abcdef";
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_Digest(input.c_str(), input.length(), hash, &hash_len, EVP_md5(), nullptr)) {
        std::string result;
        result.reserve(hash_len * 2);
        for (unsigned int i = 0; i < hash_len; i++) {
            result += hex_chars[(hash[i] >> 4) & 0x0F];
            result += hex_chars[hash[i] & 0x0F];
        }
        return result;
    }
    return "";
}

void test_md5_correctness() {
    std::cout << "Testing MD5 hex conversion logic..." << std::endl;
    std::vector<std::pair<std::string, std::string>> vectors = {
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"}
    };
    for (const auto& [input, expected] : vectors) {
        std::string result = protocolMD5_reproduction(input);
        assert(result == expected);
        assert(result.length() == 32);
    }
    std::cout << "  MD5 correctness tests passed ✓" << std::endl;
}

void test_md5_properties() {
    std::cout << "Running property-based tests for MD5..." << std::endl;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> len_dist(0, 5000);

    for (int i = 0; i < 1000; ++i) {
        std::string input;
        int len = len_dist(gen);
        for (int j = 0; j < len; ++j) input += (char)(gen() % 256);

        std::string result = protocolMD5_reproduction(input);
        std::string expected = md5Hash_reference(input);
        assert(result == expected);
    }
    std::cout << "  Property-based tests passed ✓" << std::endl;
}

int main() {
    try {
        test_md5_correctness();
        test_md5_properties();
        std::cout << "ALL PROTOCOL HASHING TESTS PASSED" << std::endl;
        return 0;
    } catch (...) {
        return 1;
    }
}
