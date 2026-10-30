// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QByteArray>
#include <QString>
#include <QObject>
#include <QFile>
#include <QPointer>
#include "accounts.h"
class QProcess;
namespace mc {
// Redact process output before it reaches disk or the log viewer.
class GameLogWriter final : public QObject {
    Q_OBJECT
public:
    explicit GameLogWriter(QProcess *process);
    QString open(const QString &path,const QStringList &secrets);
signals:
    void failed(const QString &error);
private:
    void consume(bool finish);
    QPointer<QProcess> m_process;
    QFile m_file;
    SecretRedactor m_redactor;
};
// Mojang's console configuration emits XML fragments, mixed with ordinary stderr.
class GameLogDecoder {
public:
    QString append(const QByteArray &bytes);
private:
    QByteArray m_pending;
};
}
