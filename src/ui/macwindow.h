// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once

class QWidget;

namespace mc {
// Hide only the visible caption; retain the window title for system UI.
void hideMacWindowCaption(QWidget *widget);
}
