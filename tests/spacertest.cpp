/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "core.h"
#include "definitions.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"

using namespace fakeit;

TEST_CASE("Remove all spaces", "[Spacer]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack, {1, 2});

    pCore->projectManager()->m_project = &document;
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->m_activeTimelineModel = timeline;
    pCore->projectManager()->testSetActiveDocument(&document, timeline);

    int tid1 = timeline->getTrackIndexFromPosition(2);
    int tid2 = timeline->getTrackIndexFromPosition(1);

    // Create clip with audio (40 frames long)
    QString binId = createProducer(pCore->getProjectProfile(), "red", binModel, 20);
    QString avBinId = createProducerWithSound(pCore->getProjectProfile(), binModel, 100);

    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    // Create clips in timeline
    int cid1;
    int cid2;
    int cid3;
    int cid4;
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 10, cid1));
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 80, cid2));
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 101, cid3));
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 20, cid4));

    auto state1 = [&]() {
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 80);
        REQUIRE(timeline->getClipPosition(cid3) == 101);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
    };

    SECTION("Ensure remove spaces behaves correctly")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        REQUIRE(TimelineFunctions::requestDeleteAllBlanksFrom(timeline, tid1, 20));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 30);
        REQUIRE(timeline->getClipPosition(cid3) == 50);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        state1();
    }
    SECTION("Ensure remove spaces behaves correctly with a group")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        std::unordered_set<int> ids = {cid2, cid3};
        int gid = timeline->requestClipsGroup(ids);
        REQUIRE(gid > -1);
        REQUIRE(TimelineFunctions::requestDeleteAllBlanksFrom(timeline, tid1, 20));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 30);
        REQUIRE(timeline->getClipPosition(cid3) == 51);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        state1();
        undoStack->undo();
    }
    SECTION("Ensure remove spaces behaves correctly with a group on different tracks")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        // Grouping clip at 80 on tid1 and at 20 on tid2, so the group move will be rejected for the 80 clip
        std::unordered_set<int> ids = {cid2, cid4};
        int gid = timeline->requestClipsGroup(ids);
        REQUIRE(gid > -1);
        REQUIRE(TimelineFunctions::requestDeleteAllBlanksFrom(timeline, tid1, 20));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 80);
        REQUIRE(timeline->getClipPosition(cid3) == 100);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        state1();
        undoStack->undo();
    }
    SECTION("Ensure remove spaces behaves correctly with a group on different tracks")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        // Grouping clip at 10 on tid1 and at 20 on tid2, so the clip on tid2 will be moved
        std::unordered_set<int> ids = {cid1, cid4};
        int gid = timeline->requestClipsGroup(ids);
        REQUIRE(gid > -1);
        REQUIRE(TimelineFunctions::requestDeleteAllBlanksFrom(timeline, tid1, 0));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == 20);
        REQUIRE(timeline->getClipPosition(cid3) == 40);
        REQUIRE(timeline->getClipPosition(cid4) == 10);
        undoStack->undo();
        state1();
        undoStack->undo();
    }
    SECTION("Ensure remove all clips on track behaves correctly")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        // Grouping clip at 10 on tid1 and at 20 on tid2, so the clip on tid2 will be moved
        std::unordered_set<int> ids = {cid1, cid4};
        int gid = timeline->requestClipsGroup(ids);
        REQUIRE(gid > -1);
        REQUIRE(TimelineFunctions::requestDeleteAllClipsFrom(timeline, tid1, 80));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        undoStack->undo();
        state1();
        REQUIRE(TimelineFunctions::requestDeleteAllClipsFrom(timeline, tid1, 20));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
        undoStack->undo();
        state1();
        undoStack->undo();
    }
    SECTION("Ensure delete blank works correctly with grouped clips")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        // Grouping clip at 80 and 101 on tid1
        std::unordered_set<int> ids = {cid2, cid3};
        int gid = timeline->requestClipsGroup(ids);
        REQUIRE(gid > -1);
        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 80, false) == false);
        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 70, false));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 30);
        REQUIRE(timeline->getClipPosition(cid3) == 51);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        state1();
        // Delete space between 2 grouped clips, should fail
        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 100, false) == false);
        undoStack->undo();
    }
    SECTION("Ensure delete blank works correctly with ungrouped clips")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 5, false));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == 70);
        REQUIRE(timeline->getClipPosition(cid3) == 91);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 5, true));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == 70);
        REQUIRE(timeline->getClipPosition(cid3) == 91);
        REQUIRE(timeline->getClipPosition(cid4) == 10);
        undoStack->undo();
        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 60, false));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 30);
        REQUIRE(timeline->getClipPosition(cid3) == 51);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        state1();
    }
    SECTION("Insert space test with ungrouped clips")
    {
        // We have clips at 10, 80, 101 on track 1 (length 20 frames each)
        // One clip at 20 on track 2
        // operation on one track (tid1)
        std::pair<int, int> spacerOp = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, 40);
        int cid = spacerOp.first;
        REQUIRE(cid > -1);
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        int start = timeline->getItemPosition(cid);
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, cid, start, start + 100, tid1, -1, undo, redo));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 180);
        REQUIRE(timeline->getClipPosition(cid3) == 201);
        REQUIRE(timeline->getClipPosition(cid4) == 20);
        undoStack->undo();
        state1();
        // operation on all tracks
        /*spacerOp = TimelineFunctions::requestSpacerStartOperation(timeline, -1, 35);
        cid = spacerOp.first;
        REQUIRE(cid > -1);
        start = timeline->getItemPosition(cid);
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, cid, start, start + 100, -1, -1, undo, redo));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 180);
        REQUIRE(timeline->getClipPosition(cid3) == 201);
        REQUIRE(timeline->getClipPosition(cid4) == 120);
        undoStack->undo();
        state1();*/
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}
