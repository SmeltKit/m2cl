// Copyright (C) 2026 ppm203
// GNU GPL version 3 only, with section 7 additional terms.
// See LICENSE and ADDITIONAL_TERMS.md in the repository root.

#pragma once
#include "core/packs.h"
#include <QWidget>
#include <functional>
class QTableWidget;
class QLabel;
class QLineEdit;
namespace mc {
class InstancePacksView final:public QWidget {
public:
    InstancePacksView(const Instance &instance,PackKind kind,QWidget *parent=nullptr);
    void setMutationGuard(std::function<bool()> guard){m_guard=std::move(guard);}
    std::function<void()> configureSupport;
    std::function<void()> browseMarket;
    void installResource(const ResourceProject &,const ResourceFile &);
    void refresh();
    void updateInstance(const Instance &instance) { m_instance=instance;refresh(); }
private:
    Instance m_instance;PackKind m_kind;QTableWidget *m_table;QLabel *m_status,*m_count,*m_empty;QLineEdit *m_search;QList<LocalPack> m_packs;std::function<bool()> m_guard;
    void filterRows();void importLocal();bool allowed();
};
}
