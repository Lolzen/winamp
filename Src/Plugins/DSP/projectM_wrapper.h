#pragma once

#ifdef _WIN32
    #include <windows.h>
#else
    #include <X11/Xlib.h>
    #include <GL/gl.h>
#endif

#include <projectM/projectM.h>
#include <vector>
#include <thread>
#include <atomic>

class ProjectMBridge {
public:
    ProjectMBridge() : m_visualizer(nullptr), m_running(false) {}
    ~ProjectMBridge() { Shutdown(); }

    // Use void* for the window handle to remain platform-agnostic in the interface
    bool Initialize(void* windowHandle);
    void PushAudioSamples(float* samples, int count);
    void Shutdown();

private:
    void RenderLoop();

    projectM::PmVisualizer* m_visualizer;
    void* m_windowHandle; 
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

extern "C" {
    __declspec(dllexport) void winampGetDSPPluginInfo(void* info);
    __declspec(dllexport) int winampDSPPluginInit(void* handle);
    __declspec(dllexport) int winampDSPPluginTerm(void* handle);
    __declspec(dllexport) void winampDSPPluginProcess(float* buffer, int samples);
}