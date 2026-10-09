#include "projectM_wrapper.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include <chrono>
#include <iostream>

ProjectMBridge* g_bridge = nullptr;

ProjectMBridge::ProjectMBridge() : m_windowHandle(nullptr), m_visualizer(nullptr), m_running(false) {}
ProjectMBridge::~ProjectMBridge() { Shutdown(); }

bool ProjectMBridge::Initialize(void* winampHwnd) {
    m_windowHandle = winampHwnd;
    try {
        m_visualizer = new projectM::PmVisualizer();
        m_visualizer->setPreset("default.pmpreset");
    } catch (...) {
        return false;
    }

    m_running = true;
    m_renderThread = std::thread(&ProjectMBridge::RenderLoop, this);
    return true;
}

void ProjectMBridge::PushAudioSamples(float* samples, int count) {
    if (!m_visualizer) return;
    for (int i = 0; i < count; ++i) {
        m_visualizer->pushSample(samples[i]);
    }
}

void ProjectMBridge::RenderLoop() {
    while (m_running) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if (m_visualizer) {
            m_visualizer->render();
        }

        if (m_windowHandle) {
            glXSwapBuffers((GLXWindow)m_windowHandle, 0);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void ProjectMBridge::Shutdown() {
    m_running = false;
    if (m_renderThread.joinable()) {
        m_renderThread.join();
    }
    delete m_visualizer;
    m_visualizer = nullptr;
}

// Winamp Plugin API
extern "C" {
    void winampGetDSPPluginInfo(void* info) {
        // In a real scenario, we would fill the DSPPluginInfo struct here
    }

    int winampDSPPluginInit(void* handle) {
        g_bridge = new ProjectMBridge();
        if (!g_bridge->Initialize(handle)) {
            delete g_bridge;
            g_bridge = nullptr;
            return 0; 
        }
        return 1; 
    }

    int winampDSPPluginTerm(void* handle) {
        if (g_bridge) {
            g_bridge->Shutdown();
            delete g_bridge;
            g_bridge = nullptr;
        }
        return 0;
    }

    void winampDSPPluginProcess(float* buffer, int samples) {
        if (g_bridge) {
            g_bridge->PushAudioSamples(buffer, samples);
        }
    }
}