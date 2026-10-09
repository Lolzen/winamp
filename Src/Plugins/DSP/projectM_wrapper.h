#ifndef PROJECTM_WRAPPER_H
#define PROJECTM_WRAPPER_H

#include <thread>
#include <atomic>

namespace projectM {
    class PmVisualizer;
}

class ProjectMBridge {
public:
    ProjectMBridge();
    ~ProjectMBridge();

    bool Initialize(void* winampHwnd);
    void PushAudioSamples(float* samples, int count);
    void Shutdown();

private:
    void RenderLoop();

    void* m_windowHandle;
    projectM::PmVisualizer* m_visualizer;
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

// Winamp Plugin API
extern "C" {
    void winampGetDSPPluginInfo(void* info);
    int winampDSPPluginInit(void* handle);
    int winampDSPPluginTerm(void* handle);
    void winampDSPPluginProcess(float* buffer, int samples);
}

#endif