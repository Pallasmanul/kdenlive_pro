/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_editsub_ui.h"

#include "definitions.h"


class SubtitleModel;
class TimecodeDisplay;

class ShiftEnterFilter : public QObject
{
    Q_OBJECT
public:
    ShiftEnterFilter(QObject *parent = nullptr);
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
Q_SIGNALS:
    void triggerUpdate();
};  


/**
 * @class SubtitleEdit: Subtitle edit widget
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class SubtitleEdit : public QWidget, public Ui::SubEdit_UI
{
    Q_OBJECT

public:
    explicit SubtitleEdit(QWidget *parent = nullptr);
    void setModel(std::shared_ptr<SubtitleModel> model);

public Q_SLOTS:
    void setActiveSubtitle(int id);

private Q_SLOTS:
    void updateSubtitle();
    void goToPrevious();
    void goToNext();
    void updateStyle();
    void loadStyle(const QString &style);
    void slotZoomIn();
    void slotZoomOut();

private:
    std::shared_ptr<SubtitleModel> m_model;
    int m_activeSub{-1};
    GenTime m_startPos;
    GenTime m_endPos;
    void updateCharInfo();
    void applyFontSize();

Q_SIGNALS:
    void addSubtitle(const QString &);
    void cutSubtitle(int id, int cursorPos);
};
