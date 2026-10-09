#ifndef DSP_PROJECTM_H
#define DSP_PROJECTM_H

#include "dsp_common.h"
#include <projectM/projectM.h>

class ProjectMWrapper {
public:
    ProjectMWrapper();
    ~ProjectMWrapper();
    bool Initialize();
    void ProcessAudio(float* buffer, int samples);
    void RenderFrame();

private:
    ProjectM* pm;
    bool initialized;
};

#endif