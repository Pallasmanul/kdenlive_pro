/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include <chrono>
#include <thread>

#include "bin/binplaylist.hpp"
#include "definitions.h"
#include "doc/kdenlivedoc.h"
#include "timeline2/model/builders/meltBuilder.hpp"
#include "timeline2/view/previewmanager.h"
#include "xml/xml.hpp"

TEST_CASE("Timeline preview insert-remove", "[TimelinePreview]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    // Ensure we use a progressive project profile
    pCore->setCurrentProfile("atsc_1080p_25");

    // Create document
    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    KdenliveDoc &mockedDoc = docMock.get();

    // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;
    mocked.m_project = &mockedDoc;
    QDateTime documentDate = QDateTime::currentDateTime();
    mocked.updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    mocked.m_activeTimelineModel = timeline;

    mocked.testSetActiveDocument(&mockedDoc, timeline);

    QString documentId = QString::number(QDateTime::currentMSecsSinceEpoch());
    mockedDoc.setDocumentProperty(QStringLiteral("documentid"), documentId);
    mockedDoc.setDocumentProperty(QStringLiteral("previewextension"), QStringLiteral("avi"));
    mockedDoc.setDocumentProperty(QStringLiteral("previewparameters"), QStringLiteral("vcodec=mjpeg progressive=1 qscale=10"));

    // Create base tmp folder
    bool ok = false;
    QDir dir = mockedDoc.getCacheDir(CacheBase, &ok);
    dir.mkpath(QStringLiteral("."));
    dir.mkdir(QLatin1String("preview"));

    int tid3 = timeline->getTrackIndexFromPosition(2);
    QString binId = createProducer(pCore->getProjectProfile(), "red", binModel);

    // Initialize timeline preview
    timeline->initializePreviewManager();
    timeline->buildPreviewTrack();
    REQUIRE(dir.exists(QLatin1String("preview")));
    dir.cd(QLatin1String("preview"));
    // Trigger a timeline preview
    timeline->previewManager()->addPreviewRange({0, 50}, true);
    timeline->previewManager()->startPreviewRender();

    // Wait until the preview rendering is over
    while (timeline->previewManager()->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        qDebug() << ":::: WAITING FOR PROGRESS...";
        qApp->processEvents();
    }
    QFileInfoList list = dir.entryInfoList(QDir::Files, QDir::Time);
    for (auto &file : list) {
        qDebug() << "::: FOUND FILE: " << dir.absoluteFilePath(file.fileName());
    }
    if (list.size() != 3) {
        QProcess p;
        const QString ffpath = QStandardPaths::findExecutable(QStringLiteral("melt"));
        p.start(ffpath, {QStringLiteral("-query"), QStringLiteral("formats")});
        p.waitForFinished();
        qDebug() << "::: MLT CODEC FORMATS :::\nMLT EXE: " << ffpath << "\nFORMATS:\n"
                 << p.readAllStandardOutput() << "\n----------\n"
                 << p.readAllStandardError();
    }
    // This should create 3 output chunks
    REQUIRE(list.size() == 3);

    // Create and insert clip
    int cid1 = -1;
    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;
    REQUIRE(timeline->requestClipInsertion(binId, tid3, 50, cid1, true, true, false));
    REQUIRE(timeline->getClipsCount() == 1);
    timeline->previewManager()->invalidatePreviews();
    list = dir.entryInfoList(QDir::Files, QDir::Time);
    for (auto &file : list) {
        qDebug() << "::: FOUND FILE AFTER: " << file.fileName();
    }
    // 2 chunks should remain
    REQUIRE(list.size() == 2);
    timeline->resetPreviewManager();
    // Ensure preview project folder is deleted on close
    REQUIRE(dir.exists() == false);
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
