/*
 *  VolumeCapture.h  —  software volume by capturing sound components
 */
#pragma once
#include <MacTypes.h>
#include "Status.h"

/* Capture each component listed in VolumeCapture.c behind a stand-in
   that scales the audio it passes on (startup, task time). Results per
   target go to status->cap*. */
void VolumeCaptureAttach(MmAfStatus *status);

/* The targets are registered after startup, so the wrapper calls this
   from task-time GetInfo/SetInfo until it has captured something. */
void VolumeCaptureRetry(void);
