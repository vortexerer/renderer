#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include "../Math/Vector3.h"
#include "AudioSource.h"

// Forward declarations of COM interfaces to avoid exposing SDK headers in main engine files
struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioClient;
struct IAudioRenderClient;

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Initialize();
    void Update(float dt);
    void Shutdown();

    void SetListenerPosition(const Vector3& pos) { m_ListenerPosition = pos; }
    void SetListenerVelocity(const Vector3& vel) { m_ListenerVelocity = vel; }
    const Vector3& GetListenerPosition() const { return m_ListenerPosition; }
    const Vector3& GetListenerVelocity() const { return m_ListenerVelocity; }

    void PlaySpatialSound(const std::string& clipPath, const Vector3& pos);
    std::vector<std::shared_ptr<AudioSource>> GetActiveSources();

private:
    void AudioThreadLoop();
    void MixAudio(float* outputBuffer, uint32_t numFrames);

    Vector3 m_ListenerPosition;
    Vector3 m_ListenerVelocity;
    std::vector<std::shared_ptr<AudioSource>> m_Sources;
    std::mutex m_Mutex;

    // WASAPI COM interfaces
    IMMDeviceEnumerator* m_DeviceEnumerator = nullptr;
    IMMDevice* m_Device = nullptr;
    IAudioClient* m_AudioClient = nullptr;
    IAudioRenderClient* m_RenderClient = nullptr;

    // Output Audio Parameters
    uint32_t m_SampleRate = 48000;
    uint32_t m_NumChannels = 2;
    uint32_t m_BitsPerSample = 32;

    // Streaming Thread state
    std::thread m_AudioThread;
    std::atomic<bool> m_Running{ false };
    std::atomic<bool> m_FallbackMode{ false };

    // Win32 sync events
    void* m_BufferReadyEvent = nullptr;
    void* m_ShutdownEvent = nullptr;

    // Master Limiter Envelope Follower
    float m_LimiterGain = 1.0f;
    float m_LimiterEnvelope = 0.0f;
};
