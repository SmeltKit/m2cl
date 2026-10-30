// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QStringList>

namespace mc {
struct LaunchOptions {
    bool autoMemory=true;
    int maxMemoryMiB=2048,minMemoryMiB=0;
    bool customResolution=false,fullscreen=false;
    int width=854,height=480;
    QString jvmArguments,gameArguments;
};
// Tokenize arguments only. Never invoke a shell or expand variables / command substitutions.
QStringList splitLaunchArguments(const QString &text,QString *error=nullptr);
QString validateLaunchOptions(const LaunchOptions &options);
}
