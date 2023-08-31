/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QDockWidget>

class KdenliveDoc;
class Bin;
class ProjectClip;
class ProjectItemModel;

/**
 * @namespace ClipCreationDialog
 * @brief This namespace contains a list of static methods displaying widgets
 *  allowing creation of all clip types.
 */
namespace ClipCreationDialog {

QStringList getExtensions();
QString getExtensionsFilter(const QStringList& additionalFilters = QStringList());
void createColorClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);
void createQTextClip(const QString &parentId, Bin *bin, ProjectClip *clip = nullptr);
void createAnimationClip(KdenliveDoc *doc, const QString &parentId);
void createSlideshowClip(KdenliveDoc *doc, const QString &parentId, std::shared_ptr<ProjectItemModel> model);
void createTitleClip(KdenliveDoc *doc, const QString &parentFolder, const QString &templatePath, std::shared_ptr<ProjectItemModel> model);
void createTitleTemplateClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);
void createClipsCommand(KdenliveDoc *doc, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model);
const QString createPlaylistClip(const QString &name, std::pair<int, int> tracks, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);
} // namespace ClipCreationDialog
