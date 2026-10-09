#include "projectM_wrapper.h"
#include <GL/gl.h>
#include <chrono>

#ifdef _WIN32
    #include <windowsx.h>
#else
    #include <GL/glx.h>
#endif

ProjectMBridge* g_bridge = nullptr;

bool ProjectMBridge::Initialize(void* winampHwnd) {
    m_windowHandle = winampHwnd;
    m_visualizer = new projectM::PmVisualizer();
    
    // Default preset to ensure something renders immediately
    m_visualizer->setPreset("default.pmpreset");
    
    m_running = true;
    m_renderThread = std::thread(&ProjectMBridge::RenderLoop, this);
    return true;
}

void ProjectMBridge::PushAudioSamples(float* samples, int count) {
    if (!m_visualizer) return;
    
    // projectM expects mono samples for its FFT analysis
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

        #ifdef _WIN32
            SwapBuffers(GetDC((HWND)m_windowHandle));
        #else
            // On Linux/X11, we use glXSwapBuffers. 
            // The windowHandle must be cast to a GLXWindow.
            if (m_windowHandle) {
                glXSwapBuffers((GLXWindow)m_windowHandle, 0);
            }
        #endif
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Target ~60fps
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
    // Implementation would fill the Winamp-specific DSP info struct here
}

int winampDSPPluginInit(void* handle) {
    // For simplicity, we try to grab the active window. 
    // In a real scenario, Winamp passes the main HWND via the handle.
    void* mainHwnd = nullptr;
    #ifdef _WIN32
        mainHwnd = (void*)GetForegroundWindow();
    #else
        // On Linux, the handle would be provided by the Winamp core
        mainHwnd = handle; 
    #endif
    
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