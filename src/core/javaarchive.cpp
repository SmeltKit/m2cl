// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include "javaarchive.h"
#include "gameplan.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QtEndian>
#include <zlib.h>
#include <stdexcept>

namespace mc {
namespace {
[[noreturn]] void fail(const QString &s) { throw std::runtime_error(s.toUtf8().constData()); }
void check(const JavaCancel &flag) { if (flag->load()) fail(QCoreApplication::translate("M2CL", "Java 安装已取消")); }
QString clean(QString name) { while (name.startsWith("./")) name.remove(0, 2); while (name.endsWith('/')) name.chop(1); return name; }
struct Sink {
    QString root; JavaCancel flag; quint64 total = 0; QMap<QString, QString> links;
    QString path(const QString &name) { return checkedPath(root, clean(name)); }
    void directory(const QString &name) { if (!QDir().mkpath(path(name))) fail(QCoreApplication::translate("M2CL", "无法创建 Java 目录")); }
    void link(const QString &name, const QString &target, bool hard = false) {
        const auto from = clean(name);
        if (target.startsWith('/') || target.contains('\\') || target.contains(':')) fail(QCoreApplication::translate("M2CL", "Java 压缩包链接无效"));
        const auto to = clean(QDir::cleanPath(hard ? target : QFileInfo(from).path() + "/" + target));
        path(from); path(to); links.insert(from, to);
    }
    void reserve(quint64 bytes) { check(flag); total += bytes; if (bytes > 512ULL * 1024 * 1024 || total > 2ULL * 1024 * 1024 * 1024) fail(QCoreApplication::translate("M2CL", "Java 压缩包展开超过限制")); }
    void finish() {
        for (int pass = 0; !links.isEmpty() && pass < 64; ++pass) {
            const auto entries = links;
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                check(flag); const auto source = path(it.value()), dest = path(it.key());
                if (!QFileInfo(source).isFile() || links.contains(it.value())) continue;
                if (QFileInfo::exists(dest)) fail(QCoreApplication::translate("M2CL", "Java 压缩包含重复链接"));
                reserve(QFileInfo(source).size()); QDir().mkpath(QFileInfo(dest).path());
                if (!QFile::copy(source, dest)) fail(QCoreApplication::translate("M2CL", "无法准备 Java 链接文件"));
                QFile::setPermissions(dest, QFile::permissions(source)); links.remove(it.key());
            }
        }
        if (!links.isEmpty()) fail(QCoreApplication::translate("M2CL", "Java 压缩包链接目标无效或存在循环"));
    }
};
void permissions(const QString &path, quint32 mode) {
    auto p = QFile::permissions(path) | QFile::ReadOwner | QFile::WriteOwner;
    if (mode & 0111) p |= QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther;
    QFile::setPermissions(path, p);
}
void zip(const QString &archive, Sink &sink) {
    QFile file(archive);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 512LL * 1024 * 1024) fail(QCoreApplication::translate("M2CL", "无法读取 Java ZIP"));
    const auto bytes = file.readAll(); const qint64 size = bytes.size();
    auto u16 = [&](qint64 off) { if (off < 0 || off + 2 > size) fail("Truncated ZIP"); return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + off)); };
    auto u32 = [&](qint64 off) { if (off < 0 || off + 4 > size) fail("Truncated ZIP"); return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + off)); };
    qint64 end = -1;
    for (qint64 i = size - 22; i >= qMax<qint64>(0, size - 65557); --i) if (u32(i) == 0x06054b50 && i + 22 + u16(i + 20) == size) { end = i; break; }
    if (end < 0 || u16(end + 4) || u16(end + 6) || u16(end + 10) == 65535) fail("Unsupported ZIP");
    quint64 cursor = u32(end + 16);
    for (int n = 0; n < u16(end + 10); ++n) {
        check(sink.flag);
        if (u32(cursor) != 0x02014b50) fail("Invalid ZIP directory");
        const auto flags = u16(cursor + 8), method = u16(cursor + 10);
        const auto crc = u32(cursor + 16), compressed = u32(cursor + 20), uncompressed = u32(cursor + 24), offset = u32(cursor + 42), mode = u32(cursor + 38) >> 16;
        const auto length = u16(cursor + 28); const auto name = QString::fromUtf8(bytes.mid(cursor + 46, length));
        cursor += 46 + length + u16(cursor + 30) + u16(cursor + 32);
        if (cursor > quint64(size) || flags & 1 || (method != 0 && method != 8)) fail("Unsupported ZIP entry");
        const auto path = sink.path(name);
        if (name.endsWith('/')) { sink.directory(name); continue; }
        const bool link = (mode & 0170000) == 0120000;
        if ((mode & 0170000) && !link && (mode & 0170000) != 0100000) fail("Unsafe ZIP file type");
        sink.reserve(uncompressed);
        if (u32(offset) != 0x04034b50) fail("Invalid ZIP header");
        const quint64 start = quint64(offset) + 30 + u16(offset + 26) + u16(offset + 28);
        if (start + compressed > quint64(size)) fail("Truncated ZIP data");
        QDir().mkpath(QFileInfo(path).path()); QSaveFile output(path); QByteArray target;
        if (!link && !output.open(QIODevice::WriteOnly)) fail(QCoreApplication::translate("M2CL", "无法解压 Java 文件"));
        quint64 written = 0; uLong checksum = crc32(0, nullptr, 0);
        auto consume = [&](const char *data, int amount) {
            check(sink.flag); written += amount; if (written > uncompressed) fail("ZIP size mismatch");
            checksum = crc32(checksum, reinterpret_cast<const Bytef *>(data), amount);
            if (link) { if (written > 4096) fail("Invalid ZIP link"); target.append(data, amount); }
            else if (output.write(data, amount) != amount) fail(QCoreApplication::translate("M2CL", "保存 Java 文件失败"));
        };
        if (method == 0) consume(bytes.constData() + start, compressed);
        else {
            z_stream stream{}; stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(bytes.constData() + start)); stream.avail_in = compressed;
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) fail("ZIP inflate failed");
            int status = Z_OK; char buffer[65536];
            try {
                do { stream.next_out = reinterpret_cast<Bytef *>(buffer); stream.avail_out = sizeof(buffer); status = inflate(&stream, Z_NO_FLUSH); consume(buffer, sizeof(buffer) - stream.avail_out); } while (status == Z_OK);
            } catch (...) { inflateEnd(&stream); throw; }
            const auto used = stream.total_in; inflateEnd(&stream);
            if (status != Z_STREAM_END || used != compressed) fail("Corrupt ZIP payload");
        }
        if (written != uncompressed || checksum != crc) fail("ZIP CRC mismatch");
        if (link) sink.link(name, QString::fromUtf8(target));
        else { if (!output.commit()) fail(QCoreApplication::translate("M2CL", "保存 Java 文件失败")); permissions(path, mode); }
    }
}
quint64 octal(const QByteArray &text) {
    const auto clean = text.split('\0').first().trimmed(); if (clean.isEmpty()) return 0;
    bool ok = false; const auto result = clean.toULongLong(&ok, 8); if (!ok) fail("Invalid TAR number"); return result;
}
void tar(const QString &archive, Sink &sink) {
#ifdef Q_OS_WIN
    gzFile input = gzopen_w(reinterpret_cast<const wchar_t *>(archive.utf16()), "rb");
#else
    gzFile input = gzopen(QFile::encodeName(archive).constData(), "rb");
#endif
    if (!input) fail("Cannot open TAR.gz");
    auto read = [&](int size) { check(sink.flag); QByteArray data(size, '\0'); int count = 0; while (count < size) { const int n = gzread(input, data.data() + count, size - count); if (n <= 0) fail("Truncated TAR.gz"); count += n; check(sink.flag); } return data; };
    auto string = [](const QByteArray &data) { return QString::fromUtf8(data.split('\0').first()); };
    QString longName, longLink; QMap<QString, QString> pax;
    try {
        for (int n = 0; n < 100000; ++n) {
            const auto header = read(512);
            if (header == QByteArray(512, '\0')) {
                char trailing[65536]; int amount = 0; quint64 padding = 0;
                while ((amount = gzread(input, trailing, sizeof(trailing))) > 0) {
                    check(sink.flag); padding += amount; if (padding > 1024 * 1024) fail("TAR padding too large");
                    for (int i = 0; i < amount; ++i) if (trailing[i] != 0) fail("Unexpected TAR trailing data");
                }
                int error = Z_OK; gzerror(input, &error); if (amount < 0 || (error != Z_OK && error != Z_STREAM_END)) fail("Invalid gzip checksum");
                const auto closed = gzclose(input); input = nullptr; if (closed != Z_OK) fail("Invalid gzip stream"); return;
            }
            quint64 sum = 0; for (int i = 0; i < 512; ++i) sum += i >= 148 && i < 156 ? 32 : static_cast<uchar>(header.at(i));
            if (sum != octal(header.mid(148, 8))) fail("TAR checksum mismatch");
            const auto size = octal(header.mid(124, 12)), mode = octal(header.mid(100, 8)); const char type = header.at(156);
            if (size > 512ULL * 1024 * 1024) fail("TAR entry too large");
            QString name = string(header.left(100)); const auto prefix = string(header.mid(345, 155)); if (!prefix.isEmpty()) name = prefix + "/" + name;
            if (type == 'x' || type == 'g' || type == 'L' || type == 'K') {
                if (size > 1024 * 1024) fail("TAR metadata too large"); const auto data = read(size); if (size % 512) read(512 - size % 512);
                if (type == 'L') longName = string(data); else if (type == 'K') longLink = string(data);
                else for (int i = 0; i < data.size();) {
                    const int space = data.indexOf(' ', i); if (space < i) fail("Invalid PAX record"); bool ok = false; const int len = data.mid(i, space - i).toInt(&ok);
                    if (!ok || len <= space - i + 1 || i + len > data.size()) fail("Invalid PAX length");
                    const auto record = data.mid(space + 1, i + len - space - 2); const int equals = record.indexOf('=');
                    if (equals > 0 && type == 'x') pax.insert(QString::fromUtf8(record.left(equals)), QString::fromUtf8(record.mid(equals + 1))); i += len;
                }
                continue;
            }
            if (!longName.isEmpty()) name = longName;
            if (pax.contains("path")) name = pax.value("path");
            auto target = longLink.isEmpty() ? string(header.mid(157, 100)) : longLink;
            if (pax.contains("linkpath")) target = pax.value("linkpath");
            longName.clear(); longLink.clear(); pax.clear(); name = clean(name);
            if (name.isEmpty() || name == ".") { if (size) fail("Invalid TAR root"); continue; }
            const auto path = sink.path(name); sink.reserve(size);
            if (type == '5') { if (size) fail("Invalid TAR directory"); sink.directory(name); }
            else if (type == '2' || type == '1') { if (size) fail("Invalid TAR link"); sink.link(name, target, type == '1'); }
            else if (type == '0' || type == '\0') {
                QDir().mkpath(QFileInfo(path).path()); QSaveFile output(path);
                if (!output.open(QIODevice::WriteOnly)) fail(QCoreApplication::translate("M2CL", "无法解压 Java 文件"));
                for (quint64 offset = 0; offset < size;) { const auto data = read(qMin<quint64>(65536, size - offset)); if (output.write(data) != data.size()) fail(QCoreApplication::translate("M2CL", "保存 Java 文件失败")); offset += data.size(); }
                if (size % 512) read(512 - size % 512);
                if (!output.commit()) fail(QCoreApplication::translate("M2CL", "保存 Java 文件失败")); permissions(path, mode);
            } else fail("Unsupported TAR entry");
        }
        fail("Too many TAR entries");
    } catch (...) { if (input) gzclose(input); throw; }
}
}
void extractJavaArchive(const QString &archive, const QString &directory, const JavaCancel &cancelled) {
    Sink sink{directory, cancelled, 0, {}};
    if (archive.endsWith(".zip", Qt::CaseInsensitive)) zip(archive, sink);
    else if (archive.endsWith(".tar.gz", Qt::CaseInsensitive)) tar(archive, sink);
    else fail(QCoreApplication::translate("M2CL", "不支持的 Java 压缩包格式"));
    sink.finish(); check(cancelled);
}
}
