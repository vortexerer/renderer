#include "Timer.h"
#include <cmath>
#include <chrono>

// Cross-platform high-resolution counter built on std::chrono::steady_clock.
// Counts are expressed in nanoseconds so that m_SecondsPerCount is constant
// across platforms (Windows QueryPerformanceCounter previously supplied this).
namespace {
    long long QueryCounterNanos() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
}

Timer::Timer() {
    m_SecondsPerCount = 1.0 / 1.0e9; // counter ticks are nanoseconds

    Reset();
}

float Timer::GetTotalTime() const {
    if (m_Stopped) {
        return (float)(((m_StopTime - m_PausedTime) - m_BaseTime) * m_SecondsPerCount);
    } else {
        return (float)(((m_CurrTime - m_PausedTime) - m_BaseTime) * m_SecondsPerCount);
    }
}

float Timer::GetDeltaTime() const {
    return (float)m_DeltaTime;
}

void Timer::Reset() {
    long long currTime = QueryCounterNanos();

    m_BaseTime = currTime;
    m_PrevTime = currTime;
    m_StopTime = 0;
    m_PausedTime = 0;
    m_Stopped = false;
    m_DeltaTime = 0.0;
}

void Timer::Start() {
    long long startTime = QueryCounterNanos();

    if (m_Stopped) {
        m_PausedTime += (startTime - m_StopTime);
        m_PrevTime = startTime;
        m_StopTime = 0;
        m_Stopped = false;
    }
}

void Timer::Stop() {
    if (!m_Stopped) {
        long long currTime = QueryCounterNanos();

        m_StopTime = currTime;
        m_Stopped = true;
    }
}

void Timer::Tick() {
    if (m_Stopped) {
        m_DeltaTime = 0.0;
        return;
    }

    long long currTime = QueryCounterNanos();
    m_CurrTime = currTime;

    m_DeltaTime = (m_CurrTime - m_PrevTime) * m_SecondsPerCount;
    m_PrevTime = m_CurrTime;

    if (m_DeltaTime < 0.0) {
        m_DeltaTime = 0.0;
    }
}
