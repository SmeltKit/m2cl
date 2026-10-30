// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include <QColor>
#include <QFont>
#include <QString>

namespace mc {
namespace ui {
inline constexpr int Body = 14;
inline constexpr int Secondary = 13;
inline constexpr int Caption = 12;
inline constexpr int SectionTitle = 16;
inline constexpr int PageTitle = 22;
inline constexpr int Space = 12;
inline constexpr int SectionSpace = 16;
inline constexpr int PageMargin = 24;
inline constexpr int CardMargin = 20;
}
struct Theme {
    QColor background, surface, sidebar, text, muted, border, accent, onAccent, soft;
    bool dark = false;
};
Theme makeTheme(const QString &name, bool dark);
QFont applicationFont();
void applyTheme(const Theme &theme);
}
