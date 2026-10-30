// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "catalog.h"
#include "resources.h"
#include <QByteArray>
namespace mc {
enum class PackKind { Shader, Resource };
struct LocalPack {QString fileName,name,description,problem,sha512,version;QByteArray icon;bool directory=false;int format=0;};
QString packFolder(PackKind kind);
LocalPack inspectPack(const QString &path,PackKind kind);
QList<LocalPack> localPacks(const Instance &instance,PackKind kind);
// Copies validated packages only. Existing different contents are never overwritten.
QString importPack(const Instance &instance,PackKind kind,const QString &source);
QString savePackMetadata(const Instance &,PackKind,const ResourceProject &,const ResourceFile &,const QByteArray &icon={});
QString removePack(const Instance &instance,PackKind kind,const QString &fileName);
struct ShaderSupport {QString summary;QStringList components,conflicts;bool available=false;};
ShaderSupport shaderSupport(const Instance &instance);
}
