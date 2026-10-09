#include "projectM_wrapper.h"
#include <GL/gl.h>
#include <chrono>

// Global bridge instance
ProjectMBridge* g_bridge = nullptr;

// --- projectMBridge Implementation ---

bool ProjectMBridge::Initialize(HWND winampHwnd) {
    m_hWnd = winampHwnd;
    
    // Initialize projectM core
    m_visualizer = new projectM::PmVisualizer();
    
    // Basic configuration: Load a default preset
    m_visualizer->setPreset("default.pmpreset");
    
    m_running = true;
    m_renderThread = std::thread(&ProjectMBridge::RenderLoop, this);
    
    return true;
}

void ProjectMBridge::PushAudioSamples(float* samples, int count) {
    if (!m_visualizer) return;
    
    // projectM expects mono samples for the FFT analysis
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
        
        SwapBuffers(GetDC(m_hWnd));
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
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

// --- Winamp Plugin Entry Points ---

void winampGetDSPPluginInfo(void* info) {
    // Map this to the Winamp DSP info struct in a real implementation
}

int winampDSPPluginInit(void* handle) {
    HWND mainHwnd = GetForegroundWindow(); 
    
    g_bridge = new ProjectMBridge();
    if (!g_bridge->Initialize(mainHwnd)) {
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
    return 1;
}

void winampDSPPluginProcess(float* buffer, int samples) {
    if (g_bridge) {
        g_bridge->PushAudioSamples(buffer, samples);
    }
}