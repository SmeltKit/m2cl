// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "installplandialog.h"

namespace mc {
// Offline access to project attribution and the actual texts shipped in this build.
class ProjectLegalDialog final : public InstallPlanDialog {
public:
    explicit ProjectLegalDialog(QWidget *parent = nullptr);
};
}
