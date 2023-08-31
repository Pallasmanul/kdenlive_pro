/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <QDialog>

#include "doc/documentcheckertreemodel.h"
#include "ui_missingclips_ui.h"

class DCResolveDialog : public QDialog, public Ui::MissingClips_UI
{
    Q_OBJECT

public:
    explicit DCResolveDialog(std::vector<DocumentChecker::DocumentResource> items, QWidget *parent = nullptr);

    QList<DocumentChecker::DocumentResource> getItems();

private:
    std::shared_ptr<DocumentCheckerTreeModel> m_model;

    void slotEditCurrentItem();
    void checkStatus();
    void slotRecursiveSearch();
    void setEnableChangeItems(bool enabled);
    void initProxyPanel(const std::vector<DocumentChecker::DocumentResource> &items);

    std::vector<DocumentChecker::DocumentResource> m_proxies;

private Q_SLOTS:
    void newSelection(const QItemSelection &selected, const QItemSelection &deselected);
};
