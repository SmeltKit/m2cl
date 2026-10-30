// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <cstdio>

namespace {
QString outlinedText(const QString &text, qreal x, qreal y, int size,
                     QFont::Weight weight, const QString &family, const QString &color,
                     qreal spacing = 0) {
    QFont font(family);
    font.setPixelSize(size);
    font.setWeight(weight);
    if (spacing) font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    QPainterPath path;
    path.addText(x, y, font, text);
    QString data;
    auto number = [](qreal value) { return QString::number(value, 'f', 2); };
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto point = path.elementAt(i);
        if (point.isMoveTo()) data += "M" + number(point.x) + " " + number(point.y);
        else if (point.isLineTo()) data += "L" + number(point.x) + " " + number(point.y);
        else if (point.type == QPainterPath::CurveToElement) {
            const auto control = path.elementAt(++i);
            const auto end = path.elementAt(++i);
            data += "C" + number(point.x) + " " + number(point.y) + " "
                    + number(control.x) + " " + number(control.y) + " "
                    + number(end.x) + " " + number(end.y);
        }
    }
    return QString("<path fill=\"%1\" fill-rule=\"evenodd\" d=\"%2\"/>\n").arg(color, data);
}

QString cube(qreal x, qreal y, qreal width, qreal height) {
    return QStringLiteral(R"SVG(<g transform="translate(%1 %2)">
      <path d="M0 0 %3 %4 0 %5 %6 %4Z" fill="url(#blockTop)"/>
      <path d="M%6 %4 0 %5 0 %7 %6 %8Z" fill="url(#blockLeft)"/>
      <path d="M0 %5 %3 %4 %3 %8 0 %7Z" fill="url(#blockRight)"/>
      <path d="M0 0 %3 %4 %3 %8 0 %7 %6 %8 %6 %4Z M%6 %4 0 %5 %3 %4 M0 %5V%7"
            fill="none" stroke="{{LINE}}" stroke-width="1"/>
    </g>)SVG").arg(x).arg(y).arg(width / 2).arg(width / 4).arg(width / 2)
           .arg(-width / 2).arg(width / 2 + height).arg(width / 4 + height);
}

QString banner(const QString &logo, const QString &family, bool dark) {
    const QString ink = dark ? "#e9f4ed" : "#173d31";
    const QString secondary = dark ? "#c1d6cb" : "#355c4a";
    const QString muted = dark ? "#8fac9e" : "#607d6c";
    QFont display(family);
    display.setPixelSize(104);
    display.setWeight(QFont::DemiBold);
    const qreal mWidth = QFontMetricsF(display).horizontalAdvance("M");
    QString text = outlinedText("M", 447, 202, 104, QFont::DemiBold, family, ink);
    text += outlinedText("2", 447 + mWidth + 2, 148, 46, QFont::DemiBold, family, ink);
    text += outlinedText("CL", 447 + mWidth + 36, 202, 104, QFont::DemiBold, family, ink, 1);
    text += outlinedText("Mercury MC Launcher", 451, 256, 31, QFont::Medium, family, secondary, .5);
    text += outlinedText("Minecraft: Java Edition", 452, 301, 21, QFont::Normal, family, muted, .2);
    text += outlinedText("Windows  /  macOS  /  Linux", 452, 339, 17, QFont::Medium, family, muted, .3);

    QString svg = QStringLiteral(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="400" viewBox="0 0 1440 400" role="img" aria-labelledby="title description">
  <title id="title">M²CL · Mercury MC Launcher</title>
  <desc id="description">Minecraft: Java Edition launcher. Planned platforms: Windows, macOS and Linux. Existing layered M logo with its mint orbit.</desc>
  <!-- Lettering is outlined; no external fonts, scripts or linked images. -->
  <defs>
    <linearGradient id="background" x1="0" y1="0" x2="1440" y2="400" gradientUnits="userSpaceOnUse">
      <stop stop-color="{{BG1}}"/><stop offset="1" stop-color="{{BG2}}"/>
    </linearGradient>
    <radialGradient id="halo"><stop stop-color="{{GLOW}}" stop-opacity=".19"/><stop offset="1" stop-color="{{GLOW}}" stop-opacity="0"/></radialGradient>
    <linearGradient id="divider" x2="0" y2="1"><stop stop-color="{{LINE}}" stop-opacity="0"/><stop offset=".5" stop-color="{{LINE}}" stop-opacity=".65"/><stop offset="1" stop-color="{{LINE}}" stop-opacity="0"/></linearGradient>
    <linearGradient id="blockTop" x2="0" y2="1"><stop stop-color="{{TOP}}"/><stop offset="1" stop-color="{{BOTTOM}}"/></linearGradient>
    <linearGradient id="blockLeft" x2="1" y2="1"><stop stop-color="{{LEFT}}"/><stop offset="1" stop-color="{{BOTTOM}}"/></linearGradient>
    <linearGradient id="blockRight" x2="0" y2="1"><stop stop-color="{{RIGHT}}"/><stop offset="1" stop-color="{{BOTTOM}}"/></linearGradient>
    <clipPath id="frame"><rect width="1440" height="400" rx="24"/></clipPath>
  </defs>
  <g clip-path="url(#frame)">
    <rect width="1440" height="400" fill="url(#background)"/>
    <ellipse cx="240" cy="224" rx="280" ry="254" fill="url(#halo)"/>
    <ellipse cx="1380" cy="75" rx="400" ry="280" fill="url(#halo)" opacity=".55"/>
    <g fill="none" stroke="{{LINE}}" opacity=".27">
      <path d="M1024 -32C1250 16 1480 108 1486 208C1493 332 1259 386 1095 456"/>
      <path d="M1108 -44C1314 18 1507 116 1521 209C1534 306 1375 371 1277 422"/>
      <path d="M1181 -51C1370 13 1550 126 1558 211C1567 297 1471 353 1413 403"/>
    </g>
    <g opacity="{{BLOCK_OPACITY}}">
      {{CUBES}}
    </g>
    <path d="M1076 336C1198 354 1439 278 1472 179" fill="none" stroke="{{ORBIT}}" stroke-width="1.4" opacity=".48"/>
    <circle cx="1199" cy="329" r="4.5" fill="{{ORBIT}}" opacity=".7"/>
    <path d="M395 75V346" stroke="url(#divider)"/>
    <g transform="translate(83 59) scale(.215)">{{LOGO}}</g>
    {{TEXT}}
    <rect x=".5" y=".5" width="1439" height="399" rx="23.5" fill="none" stroke="{{LINE}}" stroke-opacity=".4"/>
  </g>
</svg>
)SVG");
    svg.replace("{{LOGO}}", logo).replace("{{TEXT}}", text);
    svg.replace("{{CUBES}}", cube(1172, 125, 154, 106) + cube(1326, 71, 154, 187)
                + cube(1326, 187, 154, 70));
    const QMap<QString, QString> colors = dark ? QMap<QString, QString>{
        {"BG1", "#091c16"}, {"BG2", "#15372a"}, {"GLOW", "#89cba6"},
        {"LINE", "#598b72"}, {"TOP", "#2b5340"}, {"LEFT", "#102e22"},
        {"RIGHT", "#183f2c"}, {"BOTTOM", "#17372a"}, {"ORBIT", "#95d6ae"},
        {"BLOCK_OPACITY", ".48"}
    } : QMap<QString, QString>{
        {"BG1", "#f8faf6"}, {"BG2", "#e9f3eb"}, {"GLOW", "#acd5b5"},
        {"LINE", "#a5beab"}, {"TOP", "#d8e8d9"}, {"LEFT", "#e3eee2"},
        {"RIGHT", "#d5e5d4"}, {"BOTTOM", "#e0ecdf"}, {"ORBIT", "#4f9877"},
        {"BLOCK_OPACITY", ".65"}
    };
    for (auto it = colors.begin(); it != colors.end(); ++it)
        svg.replace("{{" + it.key() + "}}", it.value());
    return svg;
}
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    if (argc != 3) {
        std::fputs("Usage: readme_banner_renderer branding-directory preview-directory\n", stderr);
        return 1;
    }
    const QDir branding(QString::fromLocal8Bit(argv[1]));
    const QDir preview(QString::fromLocal8Bit(argv[2]));
    if (!QDir().mkpath(preview.absolutePath())) return 2;
    QFile icon(branding.filePath("m2cl.svg"));
    if (!icon.open(QIODevice::ReadOnly)) return 3;
    const QString original = QString::fromUtf8(icon.readAll());
    if (!QSvgRenderer(original.toUtf8()).isValid()) return 4;
    const auto begin = original.indexOf('>', original.indexOf("<svg")) + 1;
    const auto end = original.lastIndexOf("</svg>");
    if (begin <= 0 || end <= begin) return 5;
    const QString logo = original.mid(begin, end - begin);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto families = QFontDatabase::families();
#else
    const auto families = QFontDatabase().families();
#endif
    QString family;
    for (const auto &candidate : {"Avenir Next", "Inter", "Segoe UI", "Noto Sans", "DejaVu Sans"})
        if (families.contains(candidate)) { family = candidate; break; }
    if (family.isEmpty()) return 6;
    for (const bool dark : {false, true}) {
        const auto name = dark ? "readme-banner-dark" : "readme-banner-light";
        const auto svg = banner(logo, family, dark).toUtf8();
        QFile output(branding.filePath(QString::fromLatin1(name) + ".svg"));
        if (!output.open(QIODevice::WriteOnly) || output.write(svg) != svg.size()) return 7;
        QSvgRenderer renderer(svg);
        if (!renderer.isValid()) return 8;
        QImage image(1440, 400, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        { QPainter painter(&image); renderer.render(&painter); }
        if (!image.save(preview.filePath(QString::fromLatin1(name) + ".png"))) return 9;
    }
    std::printf("Rendered light/dark SVG banners with outlined %s lettering.\n", family.toUtf8().constData());
    return 0;
}
