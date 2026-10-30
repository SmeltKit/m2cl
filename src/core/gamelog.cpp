// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "gamelog.h"
#include <QDateTime>
#include <QXmlStreamReader>
#include <QFileInfo>
#include <QProcess>
namespace mc {
GameLogWriter::GameLogWriter(QProcess *process):QObject(process),m_process(process),m_redactor({}) {
    connect(process,&QProcess::readyReadStandardOutput,this,[this]{consume(false);});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){consume(true);});
    connect(process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart)consume(true);});
}
QString GameLogWriter::open(const QString &path,const QStringList &secrets) {
    if(QFileInfo(path).isSymLink())return QCoreApplication::translate("M2CL", "日志文件不能是符号链接");
    m_redactor=SecretRedactor(secrets);m_file.setFileName(path);
    if(!m_file.open(QIODevice::WriteOnly|QIODevice::Truncate)||!m_file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)){
        const auto error=m_file.errorString();m_file.close();return error;
    }
    return {};
}
void GameLogWriter::consume(bool finish) {
    if(!m_process)return;
    const auto bytes=m_redactor.append(m_process->readAllStandardOutput(),finish);
    if(m_file.isOpen()){
        if((!bytes.isEmpty()&&m_file.write(bytes)!=bytes.size())||!m_file.flush()){
            const auto error=m_file.errorString();m_file.close();emit failed(error);
        }
        if(finish)m_file.close();
    }
}
QString GameLogDecoder::append(const QByteArray &bytes) {
    m_pending += bytes;
    QString text;
    while (!m_pending.isEmpty()) {
        const int event = m_pending.indexOf("<log4j:Event"), newline = m_pending.indexOf('\n');
        if (event >= 0 && (newline < 0 || event < newline)) {
            if (event > 0) { const auto prefix = m_pending.left(event); if (!prefix.trimmed().isEmpty()) text += QString::fromUtf8(prefix); m_pending.remove(0, event); }
            const int end = m_pending.indexOf("</log4j:Event>");
            if (end < 0) break;
            const auto fragment = m_pending.left(end + 14); m_pending.remove(0, end + 14);
            QXmlStreamReader xml(QByteArray("<events xmlns:log4j='urn:log4j'>") + fragment + "</events>");
            QString message, details, level, thread, time;
            while (!xml.atEnd()) {
                xml.readNext(); if (!xml.isStartElement()) continue;
                if (xml.name() == QLatin1String("Event")) {
                    const auto attributes = xml.attributes(); level = attributes.value("level").toString(); thread = attributes.value("thread").toString();
                    time = QDateTime::fromMSecsSinceEpoch(attributes.value("timestamp").toLongLong()).toString("HH:mm:ss");
                } else if (xml.name() == QLatin1String("Message")) message = xml.readElementText();
                else if (xml.name() == QLatin1String("Throwable")) details = xml.readElementText();
            }
            if (xml.hasError()) text += QString::fromUtf8(fragment) + '\n';
            else { text += QString("[%1] [%2/%3] %4\n").arg(time, thread, level, message); if (!details.isEmpty()) text += details + '\n'; }
        } else {
            if (newline < 0) break;
            text += QString::fromUtf8(m_pending.left(newline + 1)); m_pending.remove(0, newline + 1);
        }
    }
    if (m_pending.size() > 1024 * 1024) { text += QString::fromUtf8(m_pending); m_pending.clear(); }
    return text;
}
}
