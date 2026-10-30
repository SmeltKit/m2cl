// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 3) { std::fputs("Usage: app_icon_renderer input.svg output.png\n", stderr); return 1; }
    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid()) return 2;
    QImage image(1024, 1024, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    { QPainter painter(&image); renderer.render(&painter); }
    return image.save(QString::fromLocal8Bit(argv[2])) ? 0 : 3;
}
