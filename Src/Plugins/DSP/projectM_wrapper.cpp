#include "projectM_wrapper.h"
#include <iostream>

ProjectMBridge::ProjectMBridge() : m_visualizer(nullptr) {}

ProjectMBridge::~ProjectMBridge() {
    Shutdown();
}

bool ProjectMBridge::Initialize(void* windowHandle) {
    try {
        // Now projectM is a class/namespace provided by the library
        m_visualizer = new projectM::PmVisualizer();
        m_visualizer->setPreset("default.pmpreset");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "ProjectM Init Error: " << e.what() << std::endl;
        return false;
    }
}

void ProjectMBridge::PushAudioSample(float* samples, int count) {
    if (!m_visualizer) return;
    
    for (int i = 0; i < count; ++i) {
        m_visualizer->pushSample(samples[i]);
    }
}

void ProjectMBridge::RenderLoop() {
    if (!m_visualizer) return;
    
    m_visualizer->render();
}

void ProjectMBridge::Shutdown() {
    if (m_visualizer) {
        delete m_visualizer;
        m_visualizer = nullptr;
    }
}