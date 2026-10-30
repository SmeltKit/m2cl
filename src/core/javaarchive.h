// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "java.h"
namespace mc {
void extractJavaArchive(const QString &archive, const QString &directory, const JavaCancel &cancelled);
}
