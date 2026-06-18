#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "../Math/Vector3.h"

class AudioSource {
public:
    AudioSource();
    ~AudioSource();

    void Play(const std::string& clipPath);
    void Stop();
    
    void SetPosition(const Vector3& pos);
    void SetVelocity(const Vector3& vel);
    Vector3 GetPosition();
    Vector3 GetVelocity();

    const std::string& GetClipPath() const { return m_ClipPath; }
    bool IsPlaying();

    // Buffer Management
    bool LoadWav(const std::string& path);
    void GenerateFallbackBuffer();

    // Playback/Streaming accessors (called by AudioEngine mixing thread)
    double GetPlaybackIndex() const { return m_PlaybackIndex; }
    void SetPlaybackIndex(double idx) { m_PlaybackIndex = idx; }
    void AdvancePlaybackIndex(double delta) { m_PlaybackIndex += delta; }
    size_t GetTotalSamples() const { return m_Samples.size(); }
    uint32_t GetSampleRate() const { return m_SampleRate; }
    const std::vector<float>& GetSamples() const { return m_Samples; }

    // Circular delay line buffering for ITD
    void PushDelaySample(float sample);
    float ReadDelayedSampleLeft(double delaySamples);
    float ReadDelayedSampleRight(double delaySamples);

private:
    Vector3 m_Position;
    Vector3 m_Velocity;
    std::string m_ClipPath;
    bool m_IsPlaying;
    std::mutex m_SourceMutex;

    // PCM Data
    std::vector<float> m_Samples;
    uint32_t m_SampleRate;

    // Playback pointer
    double m_PlaybackIndex;

    // ITD Circular Delay Lines
    static const size_t DELAY_BUFFER_SIZE = 256;
    std::vector<float> m_LeftDelayBuffer;
    std::vector<float> m_RightDelayBuffer;
    size_t m_DelayWriteCursor;
};
