// Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
#pragma once
#include "gameinstaller.h"
namespace mc {
QStringList prepareOfflineSkin(const InstallRequest &request,const JavaRuntime &java,
    const CancelFlag &cancelled,const InstallProgress &progress,QNetworkAccessManager *network=nullptr);
}
