/*
  SPDX-FileCopyrightText: 2023 Pallasmanul <pallasmanul@outlook.com>
  SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "pythoninterfaces/abstractpythoninterface.h"
#include "ui_manimwidget_ui.h"
#include <QProcess>
#include <QWidget>

#include <QListWidgetItem>
#include <QNetworkReply>

class ManimWidget : public QWidget, public Ui::ManimWidget_UI
{
    Q_OBJECT

public:
    explicit ManimWidget(QWidget *parent = nullptr);
    ~ManimWidget() override;

Q_SIGNALS:
    void addClip(const QUrl &, const QString &);
    // refreshClip
    // reloadClip
};
