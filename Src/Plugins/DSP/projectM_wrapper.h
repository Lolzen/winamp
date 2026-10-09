#ifndef PROJECTM_WRAPPER_H
#define PROJECTM_WRAPPER_H

#include <thread>
#include <atomic>

// Forward declaration of projectM types to keep the header clean
// Use the actual projectM types via include in the .cpp

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
    class PmVisualizer; // Forward declare the actual library class
    PmVisualizer* m_visualizer;
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

#endif // PROJECTM_WRAPPER_H