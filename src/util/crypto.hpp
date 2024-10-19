#pragma once

#include <string>

namespace er::util
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string base64Encode(const std::string& data);

    std::string base64Decode(const std::string& encoded_string);

    // Not really secure
    std::string encryptXOR(const std::string& text, const std::string& key);

    std::string decryptXOR(const std::string& encryptedText, const std::string& key);

    std::string encrypt(const std::string& text, const std::string& key);

    std::string decrypt(const std::string& base64EncryptedText, const std::string& key);
}