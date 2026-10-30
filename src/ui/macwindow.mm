// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#include "macwindow.h"
#include <QGuiApplication>
#include <QWidget>
#import <AppKit/AppKit.h>

namespace mc {
void hideMacWindowCaption(QWidget *widget) {
    // Offscreen tests have no NSView behind QWidget::winId().
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) return;
    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = view.window;
    if (window) {
        window.titleVisibility = NSWindowTitleHidden;
        if (widget->property("installationDialog").toBool()) {
            const QColor color = widget->palette().color(QPalette::Window);
            window.appearance = [NSAppearance appearanceNamed:color.lightnessF() < .5 ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
            window.titlebarAppearsTransparent = YES;
            window.backgroundColor = [NSColor colorWithRed:color.redF() green:color.greenF() blue:color.blueF() alpha:1.0];
        }
    }
}
}
