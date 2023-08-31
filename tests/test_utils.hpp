/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once
#include "abortutil.hpp"
#include "catch.hpp"
#include "tests_definitions.h"
#include <QString>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <mlt++/MltFactory.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>
#define private public
#define protected public
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "transitions/transitionsrepository.hpp"

using namespace fakeit;
#define RESET(mock)                                                                                                                                            \
    mock.Reset();                                                                                                                                              \
    Fake(Method(mock, adjustAssetRange));                                                                                                                      \
    Spy(Method(mock, _beginInsertRows));                                                                                                                       \
    Spy(Method(mock, _beginRemoveRows));                                                                                                                       \
    Spy(Method(mock, _endInsertRows));                                                                                                                         \
    Spy(Method(mock, _endRemoveRows));                                                                                                                         \
    Spy(OverloadedMethod(mock, notifyChange, void(const QModelIndex &, const QModelIndex &, bool, bool, bool)));                                               \
    Spy(OverloadedMethod(mock, notifyChange, void(const QModelIndex &, const QModelIndex &, const QVector<int> &)));                                           \
    Spy(OverloadedMethod(mock, notifyChange, void(const QModelIndex &, const QModelIndex &, int)));

#define NO_OTHERS()                                                                                                                                            \
    VerifyNoOtherInvocations(Method(timMock, _beginRemoveRows));                                                                                               \
    VerifyNoOtherInvocations(Method(timMock, _beginInsertRows));                                                                                               \
    VerifyNoOtherInvocations(Method(timMock, _endRemoveRows));                                                                                                 \
    VerifyNoOtherInvocations(Method(timMock, _endInsertRows));                                                                                                 \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, bool, bool, bool)));                       \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, const QVector<int> &)));                   \
    RESET(timMock);

#define CHECK_MOVE(times)                                                                                                                                      \
    Verify(Method(timMock, _beginRemoveRows) + Method(timMock, _endRemoveRows) + Method(timMock, _beginInsertRows) + Method(timMock, _endInsertRows))          \
        .Exactly(times);                                                                                                                                       \
    NO_OTHERS();

#define CHECK_INSERT(times)                                                                                                                                    \
    Verify(Method(timMock, _beginInsertRows) + Method(timMock, _endInsertRows)).Exactly(times);                                                                \
    NO_OTHERS();

#define CHECK_REMOVE(times)                                                                                                                                    \
    Verify(Method(timMock, _beginRemoveRows) + Method(timMock, _endRemoveRows)).Exactly(times);                                                                \
    NO_OTHERS();

#define CHECK_RESIZE(times)                                                                                                                                    \
    Verify(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, const QVector<int> &))).Exactly(times);                      \
    NO_OTHERS();

#define CHECK_UPDATE(role)                                                                                                                                     \
    Verify(OverloadedMethod(timMock, notifyChange, void(const QModelIndex &, const QModelIndex &, int))                                                        \
               .Matching([](const QModelIndex &, const QModelIndex &, int c) { return c == role; }))                                                           \
        .Exactly(1);                                                                                                                                           \
    NO_OTHERS();

QString createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length = 20, bool limited = true);

QString createProducerWithSound(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, int length = 10);

QString createTextProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, const QString &xmldata, const QString &clipname, int length = 10);

QString createAVProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel);

std::unique_ptr<QDomElement> getProperty(const QDomElement& element, const QString& name);
