// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.
#pragma once
#include <QImage>
#include <QJsonObject>
namespace mc {
QString skinSettingsProblem(const QJsonObject &settings);
QString skinSource(const QJsonObject &settings);
bool slimSkin(const QJsonObject &settings);
QByteArray validateSkinPng(const QByteArray &bytes, bool slim, QString *error=nullptr);
QImage expandedSkin(const QImage &image);
class SkinStore {
public:
    explicit SkinStore(QString directory):m_directory(std::move(directory)){}
    QJsonObject importPng(const QString &path,bool slim,QString *error=nullptr) const;
    QByteArray texture(const QJsonObject &settings,QString *error=nullptr) const;
    QString directory() const{return m_directory;}
private:
    QString m_directory;
};
}
