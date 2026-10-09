#include "dsp_projectm.h"
#include <iostream>

ProjectMWrapper::ProjectMWrapper() : pm(nullptr), initialized(false) {}

ProjectMWrapper::~ProjectMWrapper() {
    if (pm) {
        projectM_delete(pm);
    }
}

bool ProjectMWrapper::Initialize() {
    pm = projectM_create();
    if (!pm) return false;
    
    // Initialize projectM with default settings
    projectM_set_preset(pm, "default.pmpreset");
    initialized = true;
    return true;
}

void ProjectMWrapper::ProcessAudio(float* buffer, int samples) {
    if (!initialized || !pm) return;
    // Feed audio data to projectM
    projectM_set_audio_buffer(pm, buffer, samples);
}

void ProjectMWrapper::RenderFrame() {
    if (!initialized || !pm) return;
    // Trigger the internal render loop
    projectM_render(pm);
}