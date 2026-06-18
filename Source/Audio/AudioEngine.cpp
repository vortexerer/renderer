#include "AudioEngine.h"
#include "../Core/Logger.h"
#include "HRTFFilter.h"
#include <cmath>
#include <cstring>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

#ifndef CLSID_MMDeviceEnumerator
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
#endif
#ifndef IID_IMMDeviceEnumerator
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
#endif
#ifndef IID_IAudioClient
const IID IID_IAudioClient = __uuidof(IAudioClient);
#endif
#ifndef IID_IAudioRenderClient
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
#endif
#endif // _WIN32

AudioEngine::AudioEngine() 
    : m_ListenerPosition(0.0f, 0.0f, 0.0f)
    , m_ListenerVelocity(0.0f, 0.0f, 0.0f)
{}

AudioEngine::~AudioEngine() {
    Shutdown();
}

bool AudioEngine::Initialize() {
#ifdef _WIN32
    // 1. Initialize COM Library
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        Logger::Warning("AudioEngine: COM Initialization failed. Falling back to silent thread mode.");
        m_FallbackMode = true;
    }

    if (!m_FallbackMode) {
        // 2. Instantiate Device Enumerator
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void**>(&m_DeviceEnumerator));
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Failed to create MMDeviceEnumerator. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    if (!m_FallbackMode) {
        // 3. Get Default Endpoint
        hr = m_DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_Device);
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Default audio endpoint not found. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    if (!m_FallbackMode) {
        // 4. Activate Audio Client
        hr = m_Device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_AudioClient));
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Failed to activate IAudioClient. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    WAVEFORMATEXTENSIBLE wfx = {};
    if (!m_FallbackMode) {
        // 5. Specify format and query support
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = 2;
        wfx.Format.nSamplesPerSec = 48000;
        wfx.Format.wBitsPerSample = 32; // float
        wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = 22;
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;

        m_SampleRate = wfx.Format.nSamplesPerSec;
        m_NumChannels = wfx.Format.nChannels;
        m_BitsPerSample = 32;

        hr = m_AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, reinterpret_cast<WAVEFORMATEX*>(&wfx), nullptr);
        if (FAILED(hr)) {
            // Fall back to 16-bit PCM integer if Float PCM is unsupported
            wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            wfx.Format.wBitsPerSample = 16;
            wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
            wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
            wfx.Samples.wValidBitsPerSample = 16;
            m_BitsPerSample = 16;

            hr = m_AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, reinterpret_cast<WAVEFORMATEX*>(&wfx), nullptr);
            if (FAILED(hr)) {
                Logger::Warning("AudioEngine: Device does not support requested exclusive formats. Running in silent fallback.");
                m_FallbackMode = true;
            }
        }
    }

    if (!m_FallbackMode) {
        // 6. Initialize exclusive event-driven client
        REFERENCE_TIME period = 100000; // 10ms
        hr = m_AudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, reinterpret_cast<WAVEFORMATEX*>(&wfx), nullptr);
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Audio client initialization failed. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    // Create sync handles
    m_ShutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    
    if (!m_FallbackMode) {
        m_BufferReadyEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_BufferReadyEvent || !m_ShutdownEvent) {
            Logger::Warning("AudioEngine: Failed to create Win32 events. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    if (!m_FallbackMode) {
        hr = m_AudioClient->SetEventHandle(m_BufferReadyEvent);
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Failed to link event handle. Running in silent fallback.");
            m_FallbackMode = true;
        }

        hr = m_AudioClient->GetService(IID_IAudioRenderClient, reinterpret_cast<void**>(&m_RenderClient));
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Failed to get render service. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    // Start background worker thread
    m_Running = true;
    m_AudioThread = std::thread(&AudioEngine::AudioThreadLoop, this);

    if (!m_FallbackMode) {
        hr = m_AudioClient->Start();
        if (FAILED(hr)) {
            Logger::Warning("AudioEngine: Failed to start audio playback. Running in silent fallback.");
            m_FallbackMode = true;
        }
    }

    if (m_FallbackMode) {
        Logger::Info("AudioEngine: Running in Silent/Fallback Thread Mode.");
    } else {
        Logger::Info("AudioEngine: Initialized WASAPI Exclusive Mode audio session (" +
                     std::to_string(m_SampleRate) + "Hz, " + std::to_string(m_BitsPerSample) + "-bit Stereo).");
    }

    return true;
#else // Non-Windows: WASAPI unavailable, run portable silent fallback mixer thread
    m_FallbackMode = true;
    m_Running = true;
    m_AudioThread = std::thread(&AudioEngine::AudioThreadLoop, this);
    Logger::Info("AudioEngine: Running in Silent/Fallback Thread Mode (non-Windows platform).");
    return true;
#endif
}

void AudioEngine::Update(float dt) {
    (void)dt;
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // Prune finished audio sources
    for (auto it = m_Sources.begin(); it != m_Sources.end();) {
        if (!(*it)->IsPlaying()) {
            it = m_Sources.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioEngine::Shutdown() {
    if (m_Running) {
        m_Running = false;
#ifdef _WIN32
        if (m_ShutdownEvent) {
            SetEvent(m_ShutdownEvent);
        }
#endif
        if (m_AudioThread.joinable()) {
            m_AudioThread.join();
        }
    }

#ifdef _WIN32
    if (m_AudioClient) {
        m_AudioClient->Stop();
        m_AudioClient->Reset();
        m_AudioClient->Release();
        m_AudioClient = nullptr;
    }

    if (m_RenderClient) {
        m_RenderClient->Release();
        m_RenderClient = nullptr;
    }

    if (m_Device) {
        m_Device->Release();
        m_Device = nullptr;
    }

    if (m_DeviceEnumerator) {
        m_DeviceEnumerator->Release();
        m_DeviceEnumerator = nullptr;
    }

    if (m_BufferReadyEvent) {
        CloseHandle(m_BufferReadyEvent);
        m_BufferReadyEvent = nullptr;
    }

    if (m_ShutdownEvent) {
        CloseHandle(m_ShutdownEvent);
        m_ShutdownEvent = nullptr;
    }

    CoUninitialize();
#endif // _WIN32

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Sources.clear();
}

void AudioEngine::PlaySpatialSound(const std::string& clipPath, const Vector3& pos) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    auto src = std::make_shared<AudioSource>();
    src->SetPosition(pos);
    src->Play(clipPath);
    
    m_Sources.push_back(src);
    
    Logger::Info("AudioEngine: Playing spatial sound " + clipPath + " at position (" +
                 std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");
}

std::vector<std::shared_ptr<AudioSource>> AudioEngine::GetActiveSources() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Sources;
}

void AudioEngine::AudioThreadLoop() {
#ifdef _WIN32
    // 1. Thread Scheduling Optimization
    HANDLE hThread = GetCurrentThread();
    SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);

    DWORD taskIndex = 0;
    HANDLE hTask = nullptr;
    if (!m_FallbackMode) {
        hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    }
#endif

    uint32_t bufferSize = 480; // Default 10ms frame size for 48kHz
#ifdef _WIN32
    if (!m_FallbackMode && m_AudioClient) {
        m_AudioClient->GetBufferSize(&bufferSize);
        // Pre-roll with silence
        BYTE* pData = nullptr;
        if (SUCCEEDED(m_RenderClient->GetBuffer(bufferSize, &pData))) {
            std::memset(pData, 0, bufferSize * (m_BitsPerSample / 8) * m_NumChannels);
            m_RenderClient->ReleaseBuffer(bufferSize, 0);
        }
    }
#endif

    std::vector<float> mixBuffer(bufferSize * m_NumChannels, 0.0f);

    while (m_Running) {
        if (m_FallbackMode) {
            // In fallback mode, we sleep to emulate audio hardware period (10ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            MixAudio(mixBuffer.data(), bufferSize);
        }
#ifdef _WIN32
        else {
            HANDLE handles[2] = { m_ShutdownEvent, m_BufferReadyEvent };
            DWORD waitRes = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

            if (waitRes == WAIT_OBJECT_0) break; // Shutdown

            if (waitRes == WAIT_OBJECT_0 + 1) { // Buffer space ready
                BYTE* pData = nullptr;
                HRESULT hr = m_RenderClient->GetBuffer(bufferSize, &pData);
                if (SUCCEEDED(hr)) {
                    std::fill(mixBuffer.begin(), mixBuffer.end(), 0.0f);
                    
                    MixAudio(mixBuffer.data(), bufferSize);

                    // Convert float PCM to output audio format
                    if (m_BitsPerSample == 32) {
                        float* pFloatData = reinterpret_cast<float*>(pData);
                        std::memcpy(pFloatData, mixBuffer.data(), bufferSize * m_NumChannels * sizeof(float));
                    } else if (m_BitsPerSample == 16) {
                        int16_t* pIntData = reinterpret_cast<int16_t*>(pData);
                        for (size_t i = 0; i < mixBuffer.size(); ++i) {
                            float clampedVal = std::clamp(mixBuffer[i], -1.0f, 1.0f);
                            pIntData[i] = static_cast<int16_t>(clampedVal * 32767.0f);
                        }
                    }
                    m_RenderClient->ReleaseBuffer(bufferSize, 0);
                } else if (hr == AUDCLNT_E_RESOURCES_INVALIDATED || hr == AUDCLNT_E_DEVICE_INVALIDATED) {
                    Logger::Error("AudioEngine: WASAPI Device lost. Switching to silent fallback.");
                    m_FallbackMode = true;
                }
            }
        }
#endif
    }

#ifdef _WIN32
    if (hTask) {
        AvRevertMmThreadCharacteristics(hTask);
    }
#endif
}

void AudioEngine::MixAudio(float* outputBuffer, uint32_t numFrames) {
    // Take a local copy of active source shared pointers under a short lock
    std::vector<std::shared_ptr<AudioSource>> activeSources;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        activeSources = m_Sources;
    }

    // Local copies of listener parameters to avoid locks during loop
    Vector3 listenerPos = GetListenerPosition();
    Vector3 listenerVel = GetListenerVelocity();

    for (const auto& source : activeSources) {
        if (!source->IsPlaying()) continue;

        // Fetch source vectors
        Vector3 srcPos = source->GetPosition();
        Vector3 srcVel = source->GetVelocity();

        // 1. Calculate spatial parameters
        SpatialParameters params = HRTFFilter::CalculateSpatialParams(
            srcPos, srcVel, listenerPos, listenerVel
        );

        // Compute fractional ITD delay lengths in samples
        double leftDelaySamples = params.itdDelaySec > 0.0f ? params.itdDelaySec * m_SampleRate : 0.0;
        double rightDelaySamples = params.itdDelaySec < 0.0f ? -params.itdDelaySec * m_SampleRate : 0.0;

        // Compute playback advance steps with Doppler modulation
        double baseStep = static_cast<double>(source->GetSampleRate()) / m_SampleRate;
        double playbackStep = baseStep * params.dopplerFactor;

        size_t totalSamples = source->GetTotalSamples();
        const std::vector<float>& samples = source->GetSamples();

        for (uint32_t n = 0; n < numFrames; ++n) {
            double currentPlayIdx = source->GetPlaybackIndex();
            if (currentPlayIdx >= totalSamples) {
                source->Stop();
                break;
            }

            // 2. Resampling using Cubic Hermite Spline Interpolation
            size_t idx1 = static_cast<size_t>(currentPlayIdx);
            size_t idx0 = (idx1 > 0) ? idx1 - 1 : 0;
            size_t idx2 = (idx1 + 1 < totalSamples) ? idx1 + 1 : idx1;
            size_t idx3 = (idx1 + 2 < totalSamples) ? idx1 + 2 : idx2;
            float f = static_cast<float>(currentPlayIdx - idx1);

            float y0 = samples[idx0];
            float y1 = samples[idx1];
            float y2 = samples[idx2];
            float y3 = samples[idx3];

            // Cubic Hermite Spline interpolation
            float resampledSample = y1 + 0.5f * f * (y2 - y0 + f * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + f * (3.0f * (y1 - y2) + y3 - y0)));

            // 3. Write into source delay line
            source->PushDelaySample(resampledSample);

            // 4. Read delayed samples back with sub-sample linear interpolation
            float leftVal = source->ReadDelayedSampleLeft(leftDelaySamples);
            float rightVal = source->ReadDelayedSampleRight(rightDelaySamples);

            // 5. Apply ILD (which already contains distance attenuation)
            leftVal *= params.ildLeft;
            rightVal *= params.ildRight;

            // 6. Accumulate into master output buffer
            outputBuffer[n * 2] += leftVal;
            outputBuffer[n * 2 + 1] += rightVal;

            // Advance playback index
            source->SetPlaybackIndex(currentPlayIdx + playbackStep);
        }
    }

    // 7. Master Mix Limiter (Envelope follower with smoothed gain)
    const float attackTimeSec = 0.001f;
    const float releaseTimeSec = 0.050f;
    const float attackCoeff = std::exp(-1.0f / (attackTimeSec * m_SampleRate));
    const float releaseCoeff = std::exp(-1.0f / (releaseTimeSec * m_SampleRate));

    for (uint32_t n = 0; n < numFrames; ++n) {
        float left = outputBuffer[n * 2];
        float right = outputBuffer[n * 2 + 1];
        float peak = std::max(std::abs(left), std::abs(right));

        // Update envelope follower
        if (peak > m_LimiterEnvelope) {
            m_LimiterEnvelope = peak * (1.0f - attackCoeff) + m_LimiterEnvelope * attackCoeff;
        } else {
            m_LimiterEnvelope = peak * (1.0f - releaseCoeff) + m_LimiterEnvelope * releaseCoeff;
        }

        // Apply gain reduction threshold
        float targetGain = 1.0f;
        if (m_LimiterEnvelope > 1.0f) {
            targetGain = 1.0f / m_LimiterEnvelope;
        }

        // Smooth gain adjustment to eliminate clicks
        m_LimiterGain = m_LimiterGain * 0.9f + targetGain * 0.1f;

        outputBuffer[n * 2] *= m_LimiterGain;
        outputBuffer[n * 2 + 1] *= m_LimiterGain;
    }
}
