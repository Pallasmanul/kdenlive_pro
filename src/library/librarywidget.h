/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"

#include <QApplication>
#include <QDir>
#include <QMutex>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>

#include <KIO/ListJob>
#include <KIO/PreviewJob>
#include <KCoreDirLister>
#include <KFileItem>
#include <KMessageWidget>

class ProjectManager;
class KJob;
class QProgressBar;
class QToolBar;

/**
 * @class BinItemDelegate
 * @brief This class is responsible for drawing items in the QTreeView.
 */
class LibraryItemDelegate : public QStyledItemDelegate
{
public:
    explicit LibraryItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QRect r1 = option.rect;
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int decoWidth = int(2 * textMargin + r1.height() * 1.8);
        int mid = r1.height() / 2;
        r1.adjust(decoWidth, 0, 0, -mid);
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QRect r2 = fm.boundingRect(r1, Qt::AlignLeft | Qt::AlignTop, index.data(Qt::DisplayRole).toString()).toRect();
        editor->setGeometry(r2);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize hint = QStyledItemDelegate::sizeHint(option, index);
        QString text = index.data(Qt::UserRole + 1).toString();
        QRectF r = option.rect;
        QFont ft = option.font;
        ft.setBold(true);
        QFontMetricsF fm(ft);
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        int width = int(fm.boundingRect(r, Qt::AlignLeft | Qt::AlignTop, text).width() + option.decorationSize.width() + 2 * textMargin);
        hint.setWidth(width);
        return {hint.width(), qMax(option.fontMetrics.lineSpacing() * 2 + 4, qMax(hint.height(), option.decorationSize.height()))};
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() == 0) {
            QRect r1 = option.rect;
            painter->save();
            painter->setClipRect(r1);
            QStyleOptionViewItem opt(option);
            initStyleOption(&opt, index);
            QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
            // QRect r = QStyle::alignedRect(opt.direction, Qt::AlignVCenter | Qt::AlignLeft, opt.decorationSize, r1);

            style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);
            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.highlightedText().color());
            } else {
                painter->setPen(option.palette.text().color());
            }
            QRect r = r1;
            QFont font = painter->font();
            font.setBold(true);
            painter->setFont(font);
            int decoWidth = int(2 * textMargin + r1.height() * 1.8);
            r.setWidth(int(r1.height() * 1.8));
            // Draw thumbnail
            opt.icon.paint(painter, r);
            int mid = r1.height() / 2;
            r1.adjust(decoWidth, 0, 0, -mid);
            QRect r2 = option.rect;
            r2.adjust(decoWidth, mid, 0, 0);
            QRectF bounding;
            painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(Qt::DisplayRole).toString(), &bounding);
            font.setBold(false);
            painter->setFont(font);
            QString subText = index.data(Qt::UserRole + 1).toString();
            r2.adjust(0, int(bounding.bottom() - r2.top()), 0, 0);
            QColor subTextColor = painter->pen().color();
            subTextColor.setAlphaF(.5);
            painter->setPen(subTextColor);
            painter->drawText(r2, Qt::AlignLeft | Qt::AlignTop, subText, &bounding);
            painter->restore();
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }
};

/** @class LibraryTree
    @brief \@todo Describe class LibraryTree
    @todo Describe class LibraryTree
 */
class LibraryTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit LibraryTree(QWidget *parent = nullptr);

protected:
    QStringList mimeTypes() const override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QMimeData *mimeData(const QList<QTreeWidgetItem *> list) const override;
#else
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &list) const override;
#endif
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

public Q_SLOTS:
    void slotUpdateThumb(const QString &path, const QString &iconPath);
    void slotUpdateThumb(const QString &path, const QPixmap &pix);

Q_SIGNALS:
    void moveData(const QList<QUrl> &, const QString &);
    void importSequence(const QStringList &, const QString &);
};

/** @class LibraryWidget
    @brief A "library" that contains a list of clips to be used across projects
    @author Jean-Baptiste Mardelle
 */
class LibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryWidget(ProjectManager *m_manager, QWidget *parent = nullptr);
    void setupActions();

public Q_SLOTS:
    void slotAddToLibrary();
    void slotUpdateLibraryPath();

private Q_SLOTS:
    void slotAddToProject();
    void slotDeleteFromLibrary();
    void updateActions();
    void slotAddFolder();
    void slotRenameItem();
    void slotMoveData(const QList<QUrl> &, QString);
    void slotSaveSequence(const QStringList &info, QString dest);
    void slotItemEdited(QTreeWidgetItem *item, int column);
    void slotDownloadFinished(KJob *);
    void slotDownloadProgress(KJob *, int);
    void slotGotPreview(const KFileItem &item, const QPixmap &pix);
    void slotItemsAdded(const QUrl &url, const KFileItemList &list);
    void slotItemsDeleted(const KFileItemList &list);
    void slotClearAll();

private:
    LibraryTree *m_libraryTree;
    QToolBar *m_toolBar;
    QProgressBar *m_progressBar;
    QAction *m_addAction;
    QAction *m_deleteAction;
    QTimer m_timer;
    KMessageWidget *m_infoWidget;
    ProjectManager *m_manager;
    QList<QTreeWidgetItem *> m_folders;
    KIO::PreviewJob *m_previewJob;
    KCoreDirLister *m_coreLister;
    QMutex m_treeMutex;
    QDir m_directory;
    /** @brief if true, the next file appearing in the library will be selected */
    bool m_selectNewFile;
    void showMessage(const QString &text, KMessageWidget::MessageType type = KMessageWidget::Warning);

Q_SIGNALS:
    void addProjectClips(const QList<QUrl> &);
    void thumbReady(const QString &, const QString &);
    void enableAddSelection(bool);
    void saveTimelineSelection(QDir);
};
