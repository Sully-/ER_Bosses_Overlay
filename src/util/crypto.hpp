#include <string>

namespace er::util
{
    // Not really secure
    std::string XORCipher(const std::string& data, const std::string& key) {
        std::string result = data;
        size_t key_length = key.size();

        for (size_t i = 0; i < data.size(); ++i) {
            result[i] = data[i] ^ key[i % key_length]; 
        }

        return result;
    }
}