// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/launchoptions.h"
#include "core/memory.h"
#include <QWidget>
#include <functional>
class QCheckBox;
class QPlainTextEdit;
class QLabel;
class QRadioButton;
class QPushButton;
class QSlider;
namespace mc {
class NumberBox;
class LaunchOptionsWidget final:public QWidget {
    Q_OBJECT
public:
    explicit LaunchOptionsWidget(const LaunchOptions &options,const MemoryContext &context={},QWidget *parent=nullptr,std::function<MemorySnapshot()> sampler={});
    LaunchOptions options() const;
    QString selectionError() const;
    void setMemoryContext(const MemoryContext &context);
    void refreshRecommendation();
private:
    NumberBox *m_max,*m_min,*m_width,*m_height;
    QCheckBox *m_resolution,*m_fullscreen;
    QPlainTextEdit *m_jvm,*m_game;
    QLabel *m_error;
    QRadioButton *m_auto,*m_manual;
    QPushButton *m_recommend,*m_detailsButton;
    QLabel *m_modeBadge,*m_memoryHint,*m_basis;
    QSlider *m_slider;
    MemoryContext m_context;
    MemoryRecommendation m_recommendation;
    std::function<MemorySnapshot()> m_sampler;
    int m_manualMax;
    bool m_updating=false;
    void setAutomatic(bool enabled);
    void updateMemoryPresentation();
    void validate();
};
}
