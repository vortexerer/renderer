#pragma once
#include <string>
#include <vector>
#include <cstdint>

class TextureLoader {
public:
    static bool LoadBMP(const std::string& path, std::vector<uint8_t>& outPixels, int& outWidth, int& outHeight);
    static bool LoadTGA(const std::string& path, std::vector<uint8_t>& outPixels, int& outWidth, int& outHeight);
};
