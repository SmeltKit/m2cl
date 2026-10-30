// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QString>
namespace mc {
struct MemorySnapshot {
    qint64 totalMiB=0,availableMiB=0;
    QString source,error;
    bool estimated=false;
};
struct MemoryContext {
    QString gameVersion,gameDirectory,loader,javaArchitecture;
    int enabledModFiles=0,packSuggestedMiB=0;
    QString note;
};
struct MemoryRecommendation {
    int maximumMiB=0,targetMiB=0,budgetMiB=0,reserveMiB=0;
    QString basis,warning,error;
    bool valid() const{return error.isEmpty()&&maximumMiB>=512;}
};
MemorySnapshot systemMemory();
MemoryContext memoryContext(const QString &version,const QString &gameDirectory,const QString &loader,const QString &javaArchitecture={});
MemoryRecommendation recommendMemory(const MemorySnapshot &snapshot,const MemoryContext &context);
QString memorySize(qint64 mebibytes);
}
