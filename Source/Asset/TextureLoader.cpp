#include "TextureLoader.h"
#include <fstream>
#include "../Core/Logger.h"

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t fileType;
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offsetData;
};

struct BMPInfoHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t sizeImage;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};

struct TGAHeader {
    uint8_t idLength;
    uint8_t colorMapType;
    uint8_t dataTypeCode;
    uint16_t colorMapOrigin;
    uint16_t colorMapLength;
    uint8_t colorMapDepth;
    uint16_t xOrigin;
    uint16_t yOrigin;
    uint16_t width;
    uint16_t height;
    uint8_t bitsPerPixel;
    uint8_t imageDescriptor;
};
#pragma pack(pop)

bool TextureLoader::LoadBMP(const std::string& path, std::vector<uint8_t>& outPixels, int& outWidth, int& outHeight) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("Failed to open BMP file: " + path);
        return false;
    }

    BMPFileHeader fileHeader;
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));

    if (fileHeader.fileType != 0x4D42) { // 'BM'
        Logger::Error("Invalid BMP file magic number: " + path);
        return false;
    }

    BMPInfoHeader infoHeader;
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));

    outWidth = infoHeader.width;
    outHeight = std::abs(infoHeader.height);
    
    file.seekg(fileHeader.offsetData, std::ios::beg);

    int rowSize = ((infoHeader.bitCount * outWidth + 31) / 32) * 4;
    std::vector<uint8_t> rowData(rowSize);

    outPixels.resize(outWidth * outHeight * 4);

    for (int y = 0; y < outHeight; ++y) {
        file.read(reinterpret_cast<char*>(rowData.data()), rowSize);
        
        // BMP is stored bottom-to-top by default (positive height means bottom-to-top)
        int destY = (infoHeader.height > 0) ? (outHeight - 1 - y) : y;

        for (int x = 0; x < outWidth; ++x) {
            int srcIdx = x * (infoHeader.bitCount / 8);
            int destIdx = (destY * outWidth + x) * 4;

            if (infoHeader.bitCount == 24) {
                outPixels[destIdx + 0] = rowData[srcIdx + 2]; // R
                outPixels[destIdx + 1] = rowData[srcIdx + 1]; // G
                outPixels[destIdx + 2] = rowData[srcIdx + 0]; // B
                outPixels[destIdx + 3] = 255;                 // A
            } else if (infoHeader.bitCount == 32) {
                outPixels[destIdx + 0] = rowData[srcIdx + 2]; // R
                outPixels[destIdx + 1] = rowData[srcIdx + 1]; // G
                outPixels[destIdx + 2] = rowData[srcIdx + 0]; // B
                outPixels[destIdx + 3] = rowData[srcIdx + 3]; // A
            }
        }
    }

    return true;
}

bool TextureLoader::LoadTGA(const std::string& path, std::vector<uint8_t>& outPixels, int& outWidth, int& outHeight) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("Failed to open TGA file: " + path);
        return false;
    }

    TGAHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    // Support uncompressed true-color (type 2)
    if (header.dataTypeCode != 2) {
        Logger::Error("Unsupported TGA format (only uncompressed true-color type 2 supported): " + path);
        return false;
    }

    outWidth = header.width;
    outHeight = header.height;
    int bytesPerPixel = header.bitsPerPixel / 8;

    if (bytesPerPixel != 3 && bytesPerPixel != 4) {
        Logger::Error("Unsupported TGA bit depth: " + std::to_string(header.bitsPerPixel));
        return false;
    }

    // Skip ID info if present
    if (header.idLength > 0) {
        file.seekg(header.idLength, std::ios::cur);
    }

    int pixelCount = outWidth * outHeight;
    std::vector<uint8_t> rawData(pixelCount * bytesPerPixel);
    file.read(reinterpret_cast<char*>(rawData.data()), rawData.size());

    outPixels.resize(pixelCount * 4);

    bool flipY = !(header.imageDescriptor & 0x20); // Bit 5 controls screen origin (0 = lower left, 1 = upper left)

    for (int y = 0; y < outHeight; ++y) {
        int destY = flipY ? (outHeight - 1 - y) : y;
        for (int x = 0; x < outWidth; ++x) {
            int srcIdx = (y * outWidth + x) * bytesPerPixel;
            int destIdx = (destY * outWidth + x) * 4;

            outPixels[destIdx + 0] = rawData[srcIdx + 2]; // R
            outPixels[destIdx + 1] = rawData[srcIdx + 1]; // G
            outPixels[destIdx + 2] = rawData[srcIdx + 0]; // B
            if (bytesPerPixel == 4) {
                outPixels[destIdx + 3] = rawData[srcIdx + 3]; // A
            } else {
                outPixels[destIdx + 3] = 255;                 // A
            }
        }
    }

    return true;
}
