#ifndef PROJECTM_WRAPPER_H
#define PROJECTM_WRAPPER_H

#include <projectM.hpp> // Include the actual library header here
#include <vector>

class ProjectMBridge {
public:
    ProjectMBridge();
    ~ProjectMBridge();

    bool Initialize(void* windowHandle);
    void PushAudioSample(float* samples, int count);
    void RenderLoop();
    void Shutdown();

private:
    // Use the actual class from the library
    projectM::PmVisualizer* m_visualizer;
};

#endif