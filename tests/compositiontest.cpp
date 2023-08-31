/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"
// test specific headers
#include "doc/kdenlivedoc.h"

static QString getACompo()
{

    // Check whether repo works
    QVector<QPair<QString, QString>> transitions = TransitionsRepository::get()->getNames();
    REQUIRE(!transitions.isEmpty());

    QString aCompo;
    // Look for a compo
    for (const auto &trans : qAsConst(transitions)) {
        if (TransitionsRepository::get()->isComposition(trans.first)) {
            aCompo = trans.first;
            break;
        }
    }
    REQUIRE(!aCompo.isEmpty());
    return aCompo;
}

TEST_CASE("Basic creation/deletion of a composition", "[CompositionModel]")
{
    QString aCompo = getACompo();

    // Check construction from repo
    std::unique_ptr<Mlt::Transition> mlt_transition(TransitionsRepository::get()->getTransition(aCompo));

    REQUIRE(mlt_transition->is_valid());

    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    When(Method(docMock, getCacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    KdenliveDoc &mockedDoc = docMock.get();

    pCore->projectManager()->m_project = &mockedDoc;
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    pCore->projectManager()->m_activeTimelineModel = timeline;
    pCore->projectManager()->testSetActiveDocument(&mockedDoc, timeline);

    REQUIRE(timeline->getCompositionsCount() == 0);
    int id1 = CompositionModel::construct(timeline, aCompo, QString());
    REQUIRE(timeline->getCompositionsCount() == 1);

    int id2 = CompositionModel::construct(timeline, aCompo, QString());
    REQUIRE(timeline->getCompositionsCount() == 2);

    int id3 = CompositionModel::construct(timeline, aCompo, QString());
    REQUIRE(timeline->getCompositionsCount() == 3);

    // Test deletion
    REQUIRE(timeline->requestItemDeletion(id2));
    REQUIRE(timeline->getCompositionsCount() == 2);
    REQUIRE(timeline->requestItemDeletion(id3));
    REQUIRE(timeline->getCompositionsCount() == 1);
    REQUIRE(timeline->requestItemDeletion(id1));
    REQUIRE(timeline->getCompositionsCount() == 0);
    pCore->taskManager.slotCancelJobs();
    mockedDoc.closeTimeline(timeline->uuid());
    timeline.reset();
    pCore->projectItemModel()->clean();
}

TEST_CASE("Composition manipulation", "[CompositionModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveDoc document(undoStack, {0, 3});
    Mock<KdenliveDoc> docMock(document);
    When(Method(docMock, getCacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    KdenliveDoc &mockedDoc = docMock.get();

    pCore->projectManager()->m_project = &mockedDoc;
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    pCore->projectManager()->m_activeTimelineModel = timeline;
    pCore->projectManager()->testSetActiveDocument(&mockedDoc, timeline);

    QString aCompo = getACompo();

    int tid1 = timeline->getTrackIndexFromPosition(0);
    int tid2 = timeline->getTrackIndexFromPosition(1);
    int tid3 = timeline->getTrackIndexFromPosition(2);
    int cid2 = CompositionModel::construct(timeline, aCompo, QString());
    Q_UNUSED(tid3);
    int cid1 = CompositionModel::construct(timeline, aCompo, QString());

    REQUIRE(timeline->getCompositionPlaytime(cid1) == 1);
    REQUIRE(timeline->getCompositionPlaytime(cid2) == 1);

    SECTION("Insert a composition in a track and change track")
    {
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);

        REQUIRE(timeline->getCompositionPlaytime(cid1) == 1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == 1);
        REQUIRE(timeline->getCompositionTrackId(cid1) == -1);
        REQUIRE(timeline->getCompositionPosition(cid1) == -1);

        int pos = 10;
        REQUIRE(timeline->requestCompositionMove(cid1, tid1, pos));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == pos);
        REQUIRE(timeline->getCompositionPlaytime(cid1) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);

        pos = 10;
        REQUIRE(timeline->requestCompositionMove(cid1, tid2, pos));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid2);
        REQUIRE(timeline->getCompositionPosition(cid1) == pos);
        REQUIRE(timeline->getCompositionPlaytime(cid1) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);

        REQUIRE(timeline->requestItemResize(cid1, 10, true) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid2);
        REQUIRE(timeline->getCompositionPosition(cid1) == pos);
        REQUIRE(timeline->getCompositionPlaytime(cid1) == 10);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);

        REQUIRE(timeline->requestCompositionMove(cid2, tid2, 0));
        REQUIRE(timeline->requestItemResize(cid2, 10, true) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionPlaytime(cid2) == 10);

        // Check conflicts
        int pos2 = timeline->getCompositionPlaytime(cid1);
        REQUIRE(timeline->requestCompositionMove(cid2, tid1, pos2));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid2) == pos2);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, pos2 + 2));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid2);
        REQUIRE(timeline->getCompositionPosition(cid1) == pos);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid2) == pos2);

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, pos2 - 2));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid2);
        REQUIRE(timeline->getCompositionPosition(cid1) == pos);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid2) == pos2);

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == 0);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid2) == pos2);
    }

    SECTION("Insert consecutive compositions")
    {
        int length = 12;
        REQUIRE(timeline->requestItemResize(cid1, length, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid1) == length);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == 0);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, length));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid2) == length);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
    }

    SECTION("Resize orphan composition")
    {
        int length = 12;
        REQUIRE(timeline->requestItemResize(cid1, length, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid1) == length);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length);

        REQUIRE(timeline->getCompositionPlaytime(cid2) == length);
        REQUIRE(timeline->requestItemResize(cid2, 5, true) > -1);
        auto inOut = std::pair<int, int>{0, 4};
        REQUIRE(timeline->m_allCompositions[cid2]->getInOut() == inOut);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == 5);
        REQUIRE(timeline->requestItemResize(cid2, 10, false) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == 10);
        REQUIRE(timeline->requestItemResize(cid2, length + 1, true) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length + 1);
        REQUIRE(timeline->requestItemResize(cid2, 2, false) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == 2);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length);
        REQUIRE(timeline->requestItemResize(cid2, length - 2, true) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length - 2);
        REQUIRE(timeline->requestItemResize(cid2, length - 3, true) > -1);
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length - 3);
    }

    SECTION("Resize inserted compositions")
    {
        int length = 12;
        REQUIRE(timeline->requestItemResize(cid1, length, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());

        REQUIRE(timeline->requestItemResize(cid1, 5, true) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionPlaytime(cid1) == 5);
        REQUIRE(timeline->getCompositionPosition(cid1) == 0);

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionPlaytime(cid2) == length);
        REQUIRE(timeline->getCompositionPosition(cid2) == 5);

        REQUIRE(timeline->requestItemResize(cid1, 6, true) == -1);
        REQUIRE(timeline->requestItemResize(cid1, 6, false) == -1);
        REQUIRE(timeline->checkConsistency());

        REQUIRE(timeline->requestItemResize(cid2, length - 5, false) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionPosition(cid2) == 10);

        REQUIRE(timeline->requestItemResize(cid1, 10, true) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
    }

    SECTION("Change track of resized compositions")
    {
        int length = 12;
        REQUIRE(timeline->requestItemResize(cid1, length, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);

        REQUIRE(timeline->requestCompositionMove(cid1, tid2, 10));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 1);

        REQUIRE(timeline->requestItemResize(cid1, 5, false) > -1);
        REQUIRE(timeline->checkConsistency());

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);
    }

    SECTION("Composition Move")
    {
        int length = 12;
        REQUIRE(timeline->requestItemResize(cid1, length, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid2) == 5);

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 5 + length));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
            REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
            REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
            REQUIRE(timeline->getCompositionPosition(cid1) == 5 + length);
            REQUIRE(timeline->getCompositionPosition(cid2) == 5);
        };
        state();

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, 3 + length));
        state();

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, 0));
        state();

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, 0));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
            REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
            REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
            REQUIRE(timeline->getCompositionPosition(cid1) == 5 + length);
            REQUIRE(timeline->getCompositionPosition(cid2) == 0);
        };
        state2();

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, 0));
        state2();

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, length - 5));
        state2();

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, length));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == length);
        REQUIRE(timeline->getCompositionPosition(cid2) == 0);

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == length);
        REQUIRE(timeline->getCompositionPosition(cid2) == 0);

        // REQUIRE(timeline->allowCompositionMove(cid1, tid1, length - 5));
        REQUIRE(timeline->requestCompositionMove(cid1, tid1, length - 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == length - 5);
        REQUIRE(timeline->getCompositionPosition(cid2) == 0);

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false) > -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == length - 5);
        REQUIRE(timeline->getCompositionPosition(cid2) == 5);

        REQUIRE_FALSE(timeline->requestCompositionMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == length - 5);
        REQUIRE(timeline->getCompositionPosition(cid2) == 5);

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == length - 5);
        REQUIRE(timeline->getCompositionPosition(cid2) == 0);
    }

    SECTION("Move and resize")
    {
        int length = 12;
        REQUIRE(timeline->requestCompositionMove(cid2, tid2, 0));
        REQUIRE(timeline->requestItemResize(cid1, length, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, true) > -1);

        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 0));
        REQUIRE(timeline->getCompositionPosition(cid1) == 0);
        REQUIRE(timeline->requestItemResize(cid1, length - 2, false, 0) > -1);
        REQUIRE(timeline->getCompositionPosition(cid1) == 2);
        REQUIRE(timeline->requestCompositionMove(cid1, tid1, 0));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
            REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
            REQUIRE(timeline->getCompositionPosition(cid1) == 0);
            REQUIRE(timeline->getCompositionPlaytime(cid1) == length - 2);
        };
        state();

        REQUIRE(timeline->requestItemResize(cid1, length - 4, true) > -1);
        REQUIRE(timeline->requestCompositionMove(cid2, tid1, length - 4 + 1));
        REQUIRE(timeline->requestItemResize(cid2, length - 2, false) > -1);
        REQUIRE(timeline->requestCompositionMove(cid2, tid1, length - 4 + 1));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
            REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
            REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
            REQUIRE(timeline->getCompositionPosition(cid1) == 0);
            REQUIRE(timeline->getCompositionPlaytime(cid1) == length - 4);
            REQUIRE(timeline->getCompositionPosition(cid2) == length - 4 + 1);
            REQUIRE(timeline->getCompositionPlaytime(cid2) == length - 2);
        };
        state2();
        // the gap between the two clips is 1 frame, we try to resize them by 2 frames
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true) == -1);
        state2();
        REQUIRE(timeline->requestItemResize(cid2, length, false) == -1);
        state2();

        REQUIRE(timeline->requestCompositionMove(cid2, tid1, length - 4));
        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
            REQUIRE(timeline->getCompositionTrackId(cid2) == tid1);
            REQUIRE(timeline->getTrackCompositionsCount(tid1) == 2);
            REQUIRE(timeline->getCompositionPosition(cid1) == 0);
            REQUIRE(timeline->getCompositionPlaytime(cid1) == length - 4);
            REQUIRE(timeline->getCompositionPosition(cid2) == length - 4);
            REQUIRE(timeline->getCompositionPlaytime(cid2) == length - 2);
        };
        state3();

        // Now the gap is 0 frames, the resize should still fail
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true) == -1);
        state3();
        REQUIRE(timeline->requestItemResize(cid2, length, false) == -1);
        state3();

        // We move cid1 out of the way
        REQUIRE(timeline->requestCompositionMove(cid1, tid2, 0));
        // now resize should work
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true) > -1);
        REQUIRE(timeline->requestItemResize(cid2, length, false) > -1);
    }
    pCore->taskManager.slotCancelJobs();
    mockedDoc.closeTimeline(timeline->uuid());
    timeline.reset();
    pCore->projectItemModel()->clean();
}
