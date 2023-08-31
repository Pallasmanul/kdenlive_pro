/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "cachetask.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "utils/thumbnailcache.hpp"

#include "xml/xml.hpp"
#include <KLocalizedString>
#include <QFile>
#include <QImage>
#include <QString>
#include <QtMath>
#include <set>

CacheTask::CacheTask(const ObjectId &owner, int thumbsCount, int in, int out, QObject *object)
    : AbstractTask(owner, AbstractTask::CACHEJOB, object)
    , m_fullWidth(qFuzzyCompare(pCore->getCurrentSar(), 1.0) ? 0 : qRound(pCore->thumbProfile().height() * pCore->getCurrentDar()))
    , m_thumbsCount(thumbsCount)
    , m_in(in)
    , m_out(out)
{
    m_description = i18n("Video thumbs");
    if (m_fullWidth % 2 > 0) {
        m_fullWidth++;
    }
}

CacheTask::~CacheTask() {}

void CacheTask::start(const ObjectId &owner, int thumbsCount, int in, int out, QObject *object, bool force)
{
    if (pCore->taskManager.hasPendingJob(owner, AbstractTask::CACHEJOB)) {
        return;
    }
    CacheTask *task = new CacheTask(owner, thumbsCount, in, out, object);
    // Otherwise, start a new audio levels generation thread.
    task->m_isForce = force;
    pCore->taskManager.startTask(owner.itemId, task);
}

void CacheTask::generateThumbnail(std::shared_ptr<ProjectClip> binClip)
{
    // Fetch thumbnail
    if (binClip->clipType() != ClipType::Audio) {
        std::shared_ptr<Mlt::Producer> thumbProd(nullptr);
        int duration = m_out > 0 ? m_out - m_in : binClip->getFramePlaytime();
        std::set<int> frames;
        int steps = qCeil(qMax(pCore->getCurrentFps(), double(duration) / m_thumbsCount));
        int pos = m_in;
        for (int i = 1; i <= m_thumbsCount && pos <= m_in + duration; ++i) {
            frames.insert(pos);
            pos = m_in + (steps * i);
        }
        int size = int(frames.size());
        int count = 0;
        const QString clipId = QString::number(m_owner.itemId);
        for (int i : frames) {
            m_progress = 100 * count / size;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
            count++;
            if (m_isCanceled || pCore->taskManager.isBlocked()) {
                break;
            }
            if (ThumbnailCache::get()->hasThumbnail(clipId, i)) {
                continue;
            }
            if (thumbProd == nullptr) {
                thumbProd = binClip->thumbProducer();
            }
            if (thumbProd == nullptr) {
                // Thumb producer not available
                break;
            }
            thumbProd->seek(i);
            QScopedPointer<Mlt::Frame> frame(thumbProd->get_frame());
            if (frame != nullptr && frame->is_valid()) {
                frame->set("consumer.deinterlacer", "onefield");
                frame->set("consumer.top_field_first", -1);
                frame->set("consumer.rescale", "nearest");
                QImage result = KThumb::getFrame(frame.data(), 0, 0, m_fullWidth);
                if (!result.isNull() && !m_isCanceled) {
                    qDebug() << "==== CACHING FRAME: " << i;
                    ThumbnailCache::get()->storeThumbnail(clipId, i, result, true);
                }
            }
        }
    }
}

void CacheTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
    if (binClip) {
        generateThumbnail(binClip);
    }
    return;
}
