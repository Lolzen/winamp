#pragma once
#include <windows.h>
#include <projectM/projectM.h>
#include <vector>
#include <thread>
#include <atomic>

class ProjectMBridge {
public:
    ProjectMBridge() : m_visualizer(nullptr), m_hWnd(NULL), m_running(false) {}
    ~ProjectMBridge() { Shutdown(); }

    bool Initialize(HWND winampHwnd);
    void PushAudioSamples(float* samples, int count);
    void Shutdown();

private:
    void RenderLoop();

    projectM::PmVisualizer* m_visualizer;
    HWND m_hWnd;
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

// Winamp Plugin Interface
extern "C" {
    __declspec(dllexport) void winampGetDSPPluginInfo(void* info);
    __declspec(dllexport) int winampDSPPluginInit(void* handle);
    __declspec(dllexport) int winampDSPPluginTerm(void* handle);
    __declspec(dllexport) void winampDSPPluginProcess(float* buffer, int samples);
}