#pragma once

class Timer {
public:
    Timer();

    float GetTotalTime() const;
    float GetDeltaTime() const;

    void Reset();
    void Start();
    void Stop();
    void Tick();

private:
    double m_SecondsPerCount = 0.0;
    double m_DeltaTime = -1.0;

    long long m_BaseTime = 0;
    long long m_PausedTime = 0;
    long long m_StopTime = 0;
    long long m_PrevTime = 0;
    long long m_CurrTime = 0;

    bool m_Stopped = false;
};
