/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "thumbnailprovider.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kthumb.h"
#include "utils/thumbnailcache.hpp"

#include <QCryptographicHash>
#include <QDebug>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProfile.h>

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
}

ThumbnailProvider::~ThumbnailProvider() = default;

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage result;
    // id is binID/#frameNumber
    QString binId = id.section('/', 0, 0);
    bool ok;
    int frameNumber = id.section('#', -1).toInt(&ok);
    if (ok) {
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
        if (binClip) {
            int duration = binClip->frameDuration();
            if (frameNumber > duration) {
                // for endless loopable clips, we rewrite the position
                frameNumber = frameNumber - ((frameNumber / duration) * duration);
            }
            result = ThumbnailCache::get()->getThumbnail(binClip->hashForThumbs(), binId, frameNumber);
            if (!result.isNull()) {

                *size = result.size();
                return result;
            }
            std::shared_ptr<Mlt::Producer> prod = binClip->thumbProducer();
            if (prod && prod->is_valid()) {
                result = makeThumbnail(prod, frameNumber, requestedSize);
                ThumbnailCache::get()->storeThumbnail(binId, frameNumber, result, false);
            }
        }
    }
    if (size) *size = result.size();
    return result;
}

QString ThumbnailProvider::cacheKey(Mlt::Properties &properties, const QString &service, const QString &resource, const QString &hash, int frameNumber)
{
    QString time = properties.frames_to_time(frameNumber, mlt_time_clock);
    // Reduce the precision to centiseconds to increase chance for cache hit
    // without much loss of accuracy.
    time = time.left(time.size() - 1);
    QString key;
    if (hash.isEmpty()) {
        key = QString("%1 %2 %3").arg(service, resource, time);
        QCryptographicHash hash2(QCryptographicHash::Sha1);
        hash2.addData(key.toUtf8());
        key = hash2.result().toHex();
    } else {
        key = QString("%1 %2").arg(hash, time);
    }
    return key;
}

QImage ThumbnailProvider::makeThumbnail(const std::shared_ptr<Mlt::Producer> &producer, int frameNumber, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    producer->seek(frameNumber);
    QScopedPointer<Mlt::Frame> frame(producer->get_frame());
    if (frame == nullptr || !frame->is_valid()) {
        return QImage();
    }
    // TODO: cache these values ?
    int imageHeight = pCore->thumbProfile().height();
    int imageWidth = pCore->thumbProfile().width();
    int fullWidth = qRound(imageHeight * pCore->getCurrentDar());
    return KThumb::getFrame(frame.data(), imageWidth, imageHeight, fullWidth);
}
