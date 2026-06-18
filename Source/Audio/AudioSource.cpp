#include "AudioSource.h"
#include "../Core/Logger.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>

#pragma pack(push, 1)
struct WavHeader {
    char chunkId[4];       // "RIFF"
    uint32_t chunkSize;
    char format[4];        // "WAVE"
    char subchunk1Id[4];   // "fmt "
    uint32_t subchunk1Size;
    uint16_t audioFormat;   // 1 = PCM, 3 = IEEE Float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct DataHeader {
    char subchunk2Id[4];   // "data"
    uint32_t subchunk2Size;
};
#pragma pack(pop)

AudioSource::AudioSource() 
    : m_Position(0.0f, 0.0f, 0.0f)
    , m_Velocity(0.0f, 0.0f, 0.0f)
    , m_IsPlaying(false)
    , m_SampleRate(48000)
    , m_PlaybackIndex(0.0)
    , m_LeftDelayBuffer(DELAY_BUFFER_SIZE, 0.0f)
    , m_RightDelayBuffer(DELAY_BUFFER_SIZE, 0.0f)
    , m_DelayWriteCursor(0)
{}

AudioSource::~AudioSource() {}

void AudioSource::Play(const std::string& clipPath) {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    m_ClipPath = clipPath;
    m_PlaybackIndex = 0.0;

    if (!LoadWav(clipPath)) {
        Logger::Warning("Wav file " + clipPath + " failed to load. Generating fallback sweep.");
        GenerateFallbackBuffer();
    }

    // Reset delay lines
    std::fill(m_LeftDelayBuffer.begin(), m_LeftDelayBuffer.end(), 0.0f);
    std::fill(m_RightDelayBuffer.begin(), m_RightDelayBuffer.end(), 0.0f);
    m_DelayWriteCursor = 0;
    m_IsPlaying = true;
}

void AudioSource::Stop() {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    m_IsPlaying = false;
}

void AudioSource::SetPosition(const Vector3& pos) {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    m_Position = pos;
}

void AudioSource::SetVelocity(const Vector3& vel) {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    m_Velocity = vel;
}

Vector3 AudioSource::GetPosition() {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    return m_Position;
}

Vector3 AudioSource::GetVelocity() {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    return m_Velocity;
}

bool AudioSource::IsPlaying() {
    std::lock_guard<std::mutex> lock(m_SourceMutex);
    return m_IsPlaying;
}

bool AudioSource::LoadWav(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    WavHeader header;
    if (!file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader))) return false;

    if (std::strncmp(header.chunkId, "RIFF", 4) != 0 || std::strncmp(header.format, "WAVE", 4) != 0) {
        return false;
    }

    // Locate "data" chunk
    DataHeader dataHeader;
    while (true) {
        if (!file.read(reinterpret_cast<char*>(&dataHeader), sizeof(DataHeader))) return false;
        if (std::strncmp(dataHeader.subchunk2Id, "data", 4) == 0) break;
        
        // Skip this subchunk
        file.seekg(dataHeader.subchunk2Size, std::ios::cur);
        if (file.eof()) return false;
    }

    m_SampleRate = header.sampleRate;
    size_t numSamples = dataHeader.subchunk2Size / (header.bitsPerSample / 8);
    m_Samples.resize(numSamples / header.numChannels);

    if (header.audioFormat == 1) { // 16-bit Integer PCM
        if (header.bitsPerSample != 16) return false;
        std::vector<int16_t> rawData(numSamples);
        if (!file.read(reinterpret_cast<char*>(rawData.data()), dataHeader.subchunk2Size)) return false;

        for (size_t i = 0; i < m_Samples.size(); ++i) {
            float sum = 0.0f;
            for (int c = 0; c < header.numChannels; ++c) {
                sum += static_cast<float>(rawData[i * header.numChannels + c]) / 32768.0f;
            }
            m_Samples[i] = sum / header.numChannels;
        }
    } else if (header.audioFormat == 3) { // 32-bit Float PCM
        if (header.bitsPerSample != 32) return false;
        std::vector<float> rawData(numSamples);
        if (!file.read(reinterpret_cast<char*>(rawData.data()), dataHeader.subchunk2Size)) return false;

        for (size_t i = 0; i < m_Samples.size(); ++i) {
            float sum = 0.0f;
            for (int c = 0; c < header.numChannels; ++c) {
                sum += rawData[i * header.numChannels + c];
            }
            m_Samples[i] = sum / header.numChannels;
        }
    } else {
        return false;
    }

    return true;
}

void AudioSource::GenerateFallbackBuffer() {
    m_SampleRate = 48000;
    size_t durationSamples = 48000; // 1 second duration
    m_Samples.resize(durationSamples);
    
    // Decaying sine wave sweep to mimic collision sound (pitch drops over time)
    float baseFreq = 330.0f; 
    for (size_t i = 0; i < durationSamples; ++i) {
        float t = static_cast<float>(i) / 48000.0f;
        float freq = baseFreq * std::exp(-2.0f * t);
        float envelope = std::exp(-8.0f * t);
        m_Samples[i] = std::sin(2.0f * 3.14159265f * freq * t) * envelope;
    }
}

void AudioSource::PushDelaySample(float sample) {
    m_LeftDelayBuffer[m_DelayWriteCursor] = sample;
    m_RightDelayBuffer[m_DelayWriteCursor] = sample;
    m_DelayWriteCursor = (m_DelayWriteCursor + 1) % DELAY_BUFFER_SIZE;
}

float AudioSource::ReadDelayedSampleLeft(double delaySamples) {
    double readIdx = static_cast<double>(m_DelayWriteCursor) - delaySamples;
    while (readIdx < 0.0) readIdx += DELAY_BUFFER_SIZE;
    
    size_t i1 = static_cast<size_t>(readIdx) % DELAY_BUFFER_SIZE;
    size_t i2 = (i1 + 1) % DELAY_BUFFER_SIZE;
    float f = static_cast<float>(readIdx - static_cast<size_t>(readIdx));
    
    return (1.0f - f) * m_LeftDelayBuffer[i1] + f * m_LeftDelayBuffer[i2];
}

float AudioSource::ReadDelayedSampleRight(double delaySamples) {
    double readIdx = static_cast<double>(m_DelayWriteCursor) - delaySamples;
    while (readIdx < 0.0) readIdx += DELAY_BUFFER_SIZE;
    
    size_t i1 = static_cast<size_t>(readIdx) % DELAY_BUFFER_SIZE;
    size_t i2 = (i1 + 1) % DELAY_BUFFER_SIZE;
    float f = static_cast<float>(readIdx - static_cast<size_t>(readIdx));
    
    return (1.0f - f) * m_RightDelayBuffer[i1] + f * m_RightDelayBuffer[i2];
}
