#pragma once
#include <windows.h>

class Win32Window {
public:
    Win32Window();
    ~Win32Window();

    bool Initialize(HINSTANCE hInstance, int nCmdShow, int width, int height, const wchar_t* title);
    void PollOSMessages();
    void Shutdown();

    bool IsRunning() const { return m_IsRunning; }
    HWND GetHWND() const { return m_hWnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hWnd;
    HINSTANCE m_hInstance;
    bool m_IsRunning;
};
