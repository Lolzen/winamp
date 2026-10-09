    void* m_windowHandle;
    PmVisualizer* m_visualizer;
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

#endif // PROJECTM_WRAPPER_H
=======
#ifndef PROJECTM_WRAPPER_H
#define PROJECTM_WRAPPER_H

#include <thread>
#include <atomic>

class PmVisualizer;

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
    PmVisualizer* m_visualizer;
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

#endif // PROJECTM_WRAPPER_H
=======
    void* m_windowHandle;
    PmVisualizer* m_visualizer;
    std::atomic<bool> m_running;
    std::thread m_renderThread;
};

#endif // PROJECTM_WRAPPER_H