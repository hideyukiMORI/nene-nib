#pragma once

// The one place of the application icon's resource id (Issue #1580, ADR 0185). The ICON in
// src/app/NeNeNib.rc and the window class in EditorWindowLifecycle.cpp read it; the resource
// compiler reads it too, so it is a #define (like NeNeNibVersion.h). eng/render-app-icon.py --group
// defaults to the same value.
#define NENENIB_ICON_RESOURCE 101
