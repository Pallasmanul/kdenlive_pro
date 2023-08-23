/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "previewmanager.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/wizard.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "profiles/profilemodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "xml/xml.hpp"

#include <KLocalizedString>
#include <KMessageBox>
#include <QCollator>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

PreviewManager::PreviewManager(Mlt::Tractor *tractor, QUuid uuid, QObject *parent)
    : QObject(parent)
    , workingPreview(-1)
    , m_tractor(tractor)
    , m_uuid(uuid)
    , m_previewTrack(nullptr)
    , m_overlayTrack(nullptr)
    , m_warnOnCrash(true)
    , m_previewTrackIndex(-1)
    , m_initialized(false)
{
    m_previewGatherTimer.setSingleShot(true);
    m_previewGatherTimer.setInterval(200);
    QObject::connect(&m_previewProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &PreviewManager::processEnded);

    if (KdenliveSettings::kdenliverendererpath().isEmpty() || !QFileInfo::exists(KdenliveSettings::kdenliverendererpath())) {
        KdenliveSettings::setKdenliverendererpath(QString());
        Wizard::fixKdenliveRenderPath();
        if (KdenliveSettings::kdenliverendererpath().isEmpty()) {
            KMessageBox::error(QApplication::activeWindow(),
                               i18n("Could not find the kdenlive_render application, something is wrong with your installation. Rendering will not work"));
        }
    }

    connect(this, &PreviewManager::abortPreview, &m_previewProcess, &QProcess::kill, Qt::DirectConnection);
    connect(&m_previewProcess, &QProcess::readyReadStandardError, this, &PreviewManager::receivedStderr);
}

PreviewManager::~PreviewManager()
{
    if (m_initialized) {
        abortRendering();
        if (m_undoDir.dirName() == QLatin1String("undo")) {
            m_undoDir.removeRecursively();
        }
        if ((pCore->currentDoc()->url().isEmpty() && m_cacheDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) ||
            m_cacheDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            if (m_cacheDir.dirName() == QLatin1String("preview")) {
                m_cacheDir.removeRecursively();
            }
        }
    }
    delete m_overlayTrack;
    delete m_previewTrack;
}

bool PreviewManager::initialize()
{
    // Make sure our document id does not contain .. tricks
    bool ok;
    KdenliveDoc *doc = pCore->currentDoc();
    QString documentId = QDir::cleanPath(doc->getDocumentProperty(QStringLiteral("documentid")));
    documentId.toLongLong(&ok, 10);
    if (!ok || documentId.isEmpty()) {
        // Something is wrong, documentId should be a number (ms since epoch), abort
        pCore->displayMessage(i18n("Wrong document ID, cannot create temporary folder"), ErrorMessage);
        return false;
    }
    m_cacheDir = doc->getCacheDir(CachePreview, &ok, m_uuid);
    qDebug() << ":: GOT CACHE DIR: " << m_cacheDir.absolutePath();
    if (!ok || !m_cacheDir.exists()) {
        pCore->displayMessage(i18n("Cannot read folder %1", m_cacheDir.absolutePath()), ErrorMessage);
        return false;
    }
    if (m_uuid == doc->uuid()) {
        if (m_cacheDir.dirName() != QLatin1String("preview") || m_cacheDir == QDir() ||
            (!m_cacheDir.exists(QStringLiteral("undo")) && !m_cacheDir.mkdir(QStringLiteral("undo"))) || !m_cacheDir.absolutePath().contains(documentId)) {
            pCore->displayMessage(i18n("Something is wrong with cache folder %1", m_cacheDir.absolutePath()), ErrorMessage);
            return false;
        }
    } else {
        if (m_cacheDir.dirName().toLatin1() != QCryptographicHash::hash(m_uuid.toByteArray(), QCryptographicHash::Md5).toHex() || m_cacheDir == QDir() ||
            (!m_cacheDir.exists(QStringLiteral("undo")) && !m_cacheDir.mkdir(QStringLiteral("undo"))) || !m_cacheDir.absolutePath().contains(documentId)) {
            pCore->displayMessage(i18n("Something is wrong with cache folder %1", m_cacheDir.absolutePath()), ErrorMessage);
            return false;
        }
    }
    if (!loadParams()) {
        pCore->displayMessage(i18n("Invalid timeline preview parameters"), ErrorMessage);
        return false;
    }
    m_undoDir = QDir(m_cacheDir.absoluteFilePath(QStringLiteral("undo")));

    // Make sure our cache dirs are inside the temporary folder
    if (!m_cacheDir.makeAbsolute() || !m_undoDir.makeAbsolute() || !m_undoDir.mkpath(QStringLiteral("."))) {
        pCore->displayMessage(i18n("Something is wrong with cache folders"), ErrorMessage);
        return false;
    }

    connect(this, &PreviewManager::cleanupOldPreviews, this, &PreviewManager::doCleanupOldPreviews);
    connect(doc, &KdenliveDoc::removeInvalidUndo, this, &PreviewManager::slotRemoveInvalidUndo, Qt::DirectConnection);
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(3000);
    connect(&m_previewTimer, &QTimer::timeout, this, &PreviewManager::startPreviewRender);
    connect(this, &PreviewManager::previewRender, this, &PreviewManager::gotPreviewRender, Qt::DirectConnection);
    connect(&m_previewGatherTimer, &QTimer::timeout, this, &PreviewManager::slotProcessDirtyChunks);
    m_initialized = true;
    return true;
}

bool PreviewManager::buildPreviewTrack()
{
    if (m_previewTrack != nullptr) {
        return false;
    }
    // Create overlay track
    qDebug() << "/// BUILDING PREVIEW TRACK\n----------------------\n----------------__";
    m_previewTrack = new Mlt::Playlist(pCore->getProjectProfile());
    m_previewTrack->set("kdenlive:playlistid", "timeline_preview");
    m_tractor->lock();
    reconnectTrack();
    m_tractor->unlock();
    return true;
}

void PreviewManager::loadChunks(QVariantList previewChunks, QVariantList dirtyChunks, Mlt::Playlist &playlist)
{
    if (previewChunks.isEmpty()) {
        previewChunks = m_renderedChunks;
    }
    if (dirtyChunks.isEmpty()) {
        dirtyChunks = m_dirtyChunks;
    }

    QStringList existingChuncks;
    if (!previewChunks.isEmpty()) {
        existingChuncks = m_cacheDir.entryList(QDir::Files);
    }

    int max = playlist.count();
    std::shared_ptr<Mlt::Producer> clip;
    m_tractor->lock();
    if (max == 0) {
        // Empty timeline preview, mark all as dirty
        for (auto &prev : previewChunks) {
            dirtyChunks << prev;
        }
    }
    for (int i = 0; i < max; i++) {
        if (playlist.is_blank(i)) {
            continue;
        }
        int position = playlist.clip_start(i);
        if (previewChunks.contains(QString::number(position))) {
            if (existingChuncks.contains(QString("%1.%2").arg(position).arg(m_extension))) {
                clip.reset(playlist.get_clip(i));
                m_renderedChunks << position;
                m_previewTrack->insert_at(position, clip.get(), 1);
            } else {
                dirtyChunks << position;
            }
        }
    }
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
    if (!dirtyChunks.isEmpty()) {
        std::sort(dirtyChunks.begin(), dirtyChunks.end(), chunkSort);
        QMutexLocker lock(&m_dirtyMutex);
        for (const auto &i : qAsConst(dirtyChunks)) {
            if (!m_dirtyChunks.contains(i)) {
                m_dirtyChunks << i;
            }
        }
        Q_EMIT dirtyChunksChanged();
    }
    if (!previewChunks.isEmpty()) {
        Q_EMIT renderedChunksChanged();
    }
}

void PreviewManager::deletePreviewTrack()
{
    m_tractor->lock();
    disconnectTrack();
    delete m_previewTrack;
    m_previewTrack = nullptr;
    m_dirtyChunks.clear();
    m_renderedChunks.clear();
    Q_EMIT dirtyChunksChanged();
    Q_EMIT renderedChunksChanged();
    m_tractor->unlock();
}

const QDir PreviewManager::getCacheDir() const
{
    return m_cacheDir;
}

void PreviewManager::reconnectTrack()
{
    disconnectTrack();
    if (!m_previewTrack && !m_overlayTrack) {
        m_previewTrackIndex = -1;
        return;
    }
    m_previewTrackIndex = m_tractor->count();
    int increment = 0;
    if (m_previewTrack) {
        m_tractor->insert_track(*m_previewTrack, m_previewTrackIndex);
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(m_previewTrackIndex));
        tk->set("hide", 2);
        // tk->set("kdenlive:playlistid", "timeline_preview");
        increment++;
    }
    if (m_overlayTrack) {
        m_tractor->insert_track(*m_overlayTrack, m_previewTrackIndex + increment);
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(m_previewTrackIndex + increment));
        tk->set("hide", 2);
        // tk->set("kdenlive:playlistid", "timeline_overlay");
    }
}

void PreviewManager::disconnectTrack()
{
    if (m_previewTrackIndex > -1) {
        Mlt::Producer *prod = m_tractor->track(m_previewTrackIndex);
        if (strcmp(prod->get("kdenlive:playlistid"), "timeline_preview") == 0 || strcmp(prod->get("kdenlive:playlistid"), "timeline_overlay") == 0) {
            m_tractor->remove_track(m_previewTrackIndex);
        }
        delete prod;
        if (m_tractor->count() == m_previewTrackIndex + 1) {
            // overlay track still here, remove
            Mlt::Producer *trkprod = m_tractor->track(m_previewTrackIndex);
            if (strcmp(trkprod->get("kdenlive:playlistid"), "timeline_overlay") == 0) {
                m_tractor->remove_track(m_previewTrackIndex);
            }
            delete trkprod;
        }
    }
    m_previewTrackIndex = -1;
}

void PreviewManager::disable()
{
    if (m_previewTrackIndex > -1) {
        if (m_previewTrack) {
            m_previewTrack->set("hide", 3);
        }
        if (m_overlayTrack) {
            m_overlayTrack->set("hide", 3);
        }
    }
}

void PreviewManager::enable()
{
    if (m_previewTrackIndex > -1) {
        if (m_previewTrack) {
            m_previewTrack->set("hide", 2);
        }
        if (m_overlayTrack) {
            m_overlayTrack->set("hide", 2);
        }
    }
}

bool PreviewManager::loadParams()
{
    KdenliveDoc *doc = pCore->currentDoc();
    m_extension = doc->getDocumentProperty(QStringLiteral("previewextension"));
    m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), Qt::SkipEmptyParts);

    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        doc->selectPreviewProfile();
        m_consumerParams = doc->getDocumentProperty(QStringLiteral("previewparameters")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        m_extension = doc->getDocumentProperty(QStringLiteral("previewextension"));
    }
    if (m_consumerParams.isEmpty() || m_extension.isEmpty()) {
        return false;
    }
    // Remove the r= and s= parameter (forcing framerate / frame size) as it causes rendering failure.
    // These parameters should be provided by MLT's profile
    // NOTE: this is still required for DNxHD so leave it
    /*for (int i = 0; i < m_consumerParams.count(); i++) {
        if (m_consumerParams.at(i).startsWith(QStringLiteral("r=")) || m_consumerParams.at(i).startsWith(QStringLiteral("s="))) {
            m_consumerParams.removeAt(i);
            i--;
        }
    }*/
    if (doc->getDocumentProperty(QStringLiteral("resizepreview")).toInt() != 0) {
        int resizeWidth = doc->getDocumentProperty(QStringLiteral("previewheight")).toInt();
        m_consumerParams << QStringLiteral("s=%1x%2").arg(int(resizeWidth * pCore->getCurrentDar())).arg(resizeWidth);
    }
    m_consumerParams << QStringLiteral("an=1");
    if (KdenliveSettings::gpu_accel()) {
        m_consumerParams << QStringLiteral("glsl.=1");
    }
    return true;
}

void PreviewManager::invalidatePreviews()
{
    QMutexLocker lock(&m_previewMutex);
    bool timer = KdenliveSettings::autopreview();
    if (m_previewTimer.isActive()) {
        m_previewTimer.stop();
        timer = true;
    }
    KdenliveDoc *doc = pCore->currentDoc();
    int stackIx = doc->commandStack()->index();
    int stackMax = doc->commandStack()->count();
    if (stackIx == stackMax && !m_undoDir.exists(QString::number(stackIx - 1))) {
        // We just added a new command in stack, archive existing chunks
        int ix = stackIx - 1;
        m_undoDir.mkdir(QString::number(ix));
        bool foundPreviews = false;
        for (const auto &i : m_dirtyChunks) {
            QString current = QStringLiteral("%1.%2").arg(i.toInt()).arg(m_extension);
            if (m_cacheDir.rename(current, QStringLiteral("undo/%1/%2").arg(ix).arg(current))) {
                foundPreviews = true;
            }
        }
        if (!foundPreviews) {
            // No preview files found, remove undo folder
            m_undoDir.rmdir(QString::number(ix));
        } else {
            // new chunks archived, cleanup old ones
            Q_EMIT cleanupOldPreviews();
        }
    } else {
        // Restore existing chunks, delete others
        // Check if we just undo the last stack action, then backup, otherwise delete
        bool lastUndo = false;
        if (stackIx + 1 == stackMax) {
            if (!m_undoDir.exists(QString::number(stackMax))) {
                lastUndo = true;
                bool foundPreviews = false;
                m_undoDir.mkdir(QString::number(stackMax));
                for (const auto &i : m_dirtyChunks) {
                    QString current = QStringLiteral("%1.%2").arg(i.toInt()).arg(m_extension);
                    if (m_cacheDir.rename(current, QStringLiteral("undo/%1/%2").arg(stackMax).arg(current))) {
                        foundPreviews = true;
                    }
                }
                if (!foundPreviews) {
                    m_undoDir.rmdir(QString::number(stackMax));
                }
            }
        }
        bool moveFile = true;
        QDir tmpDir = m_undoDir;
        if (!tmpDir.cd(QString::number(stackIx))) {
            moveFile = false;
        }
        QVariantList foundChunks;
        for (const auto &i : m_dirtyChunks) {
            QString cacheFileName = QStringLiteral("%1.%2").arg(i.toInt()).arg(m_extension);
            if (!lastUndo) {
                m_cacheDir.remove(cacheFileName);
            }
            if (moveFile) {
                if (QFile::copy(tmpDir.absoluteFilePath(cacheFileName), m_cacheDir.absoluteFilePath(cacheFileName))) {
                    foundChunks << i;
                } else {
                    qDebug() << "// ERROR PROCESSE CHUNK: " << i << ", " << cacheFileName;
                }
            }
        }
        if (!foundChunks.isEmpty()) {
            std::sort(foundChunks.begin(), foundChunks.end(), chunkSort);
            m_dirtyMutex.lock();
            for (auto &ck : foundChunks) {
                m_dirtyChunks.removeAll(ck);
                m_renderedChunks << ck;
            }
            m_dirtyMutex.unlock();
            Q_EMIT dirtyChunksChanged();
            Q_EMIT renderedChunksChanged();
            reloadChunks(foundChunks);
        }
    }
    doc->setModified(true);
    if (timer) {
        m_previewTimer.start();
    }
}

void PreviewManager::doCleanupOldPreviews()
{
    if (m_undoDir.dirName() != QLatin1String("undo")) {
        return;
    }
    QStringList dirs = m_undoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // Use QCollator to do a natural sorting so that 10 is after 2
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(dirs.begin(), dirs.end(), [&collator](const QString &file1, const QString &file2) { return collator.compare(file1, file2) < 0; });
    bool ok;
    while (dirs.count() > 5) {
        QDir tmp = m_undoDir;
        QString dirName = dirs.takeFirst();
        dirName.toInt(&ok);
        if (ok && tmp.cd(dirName)) {
            tmp.removeRecursively();
        }
    }
}

void PreviewManager::clearPreviewRange(bool resetZones)
{
    m_previewGatherTimer.stop();
    abortRendering();
    m_tractor->lock();
    bool hasPreview = m_previewTrack != nullptr;
    QMutexLocker lock(&m_dirtyMutex);
    for (const auto &ix : qAsConst(m_renderedChunks)) {
        m_cacheDir.remove(QStringLiteral("%1.%2").arg(ix.toInt()).arg(m_extension));
        if (!m_dirtyChunks.contains(ix)) {
            m_dirtyChunks << ix;
        }
        if (!hasPreview) {
            continue;
        }
        int trackIx = m_previewTrack->get_clip_index_at(ix.toInt());
        if (!m_previewTrack->is_blank(trackIx)) {
            Mlt::Producer *prod = m_previewTrack->replace_with_blank(trackIx);
            delete prod;
        }
    }
    if (hasPreview) {
        m_previewTrack->consolidate_blanks();
    }
    m_tractor->unlock();
    m_renderedChunks.clear();
    // Reload preview params
    loadParams();
    if (resetZones) {
        m_dirtyChunks.clear();
    }
    Q_EMIT renderedChunksChanged();
    Q_EMIT dirtyChunksChanged();
}

void PreviewManager::addPreviewRange(const QPoint zone, bool add)
{
    int chunkSize = KdenliveSettings::timelinechunks();
    int startChunk = zone.x() / chunkSize;
    int endChunk = int(rintl(zone.y() / chunkSize));
    QList<int> toRemove;
    QMutexLocker lock(&m_dirtyMutex);
    for (int i = startChunk; i <= endChunk; i++) {
        int frame = i * chunkSize;
        if (add) {
            if (!m_renderedChunks.contains(frame) && !m_dirtyChunks.contains(frame)) {
                m_dirtyChunks << frame;
            }
        } else {
            if (m_renderedChunks.contains(frame)) {
                toRemove << frame;
                m_renderedChunks.removeAll(frame);
            } else {
                m_dirtyChunks.removeAll(frame);
            }
        }
    }
    if (add) {
        Q_EMIT dirtyChunksChanged();
        if (m_previewProcess.state() == QProcess::NotRunning && KdenliveSettings::autopreview()) {
            m_previewTimer.start();
        }
    } else {
        // Remove processed chunks
        bool isRendering = m_previewProcess.state() != QProcess::NotRunning;
        m_previewGatherTimer.stop();
        abortRendering();
        m_tractor->lock();
        bool hasPreview = m_previewTrack != nullptr;
        for (int ix : qAsConst(toRemove)) {
            m_cacheDir.remove(QStringLiteral("%1.%2").arg(ix).arg(m_extension));
            if (!hasPreview) {
                continue;
            }
            int trackIx = m_previewTrack->get_clip_index_at(ix);
            if (!m_previewTrack->is_blank(trackIx)) {
                Mlt::Producer *prod = m_previewTrack->replace_with_blank(trackIx);
                delete prod;
            }
        }
        if (hasPreview) {
            m_previewTrack->consolidate_blanks();
        }
        Q_EMIT renderedChunksChanged();
        Q_EMIT dirtyChunksChanged();
        m_tractor->unlock();
        if (isRendering || KdenliveSettings::autopreview()) {
            m_previewTimer.start();
        }
    }
}

void PreviewManager::abortRendering()
{
    if (m_previewProcess.state() == QProcess::NotRunning) {
        return;
    }
    // Don't display error message on voluntary abort
    m_warnOnCrash = false;
    Q_EMIT abortPreview();
    m_previewProcess.waitForFinished();
    if (m_previewProcess.state() != QProcess::NotRunning) {
        m_previewProcess.kill();
        m_previewProcess.waitForFinished();
    }
    // Re-init time estimation
    Q_EMIT previewRender(-1, QString(), 1000);
}

bool PreviewManager::hasDefinedRange() const
{
    return (!m_renderedChunks.isEmpty() || !m_dirtyChunks.isEmpty());
}

void PreviewManager::startPreviewRender()
{
    QMutexLocker lock(&m_previewMutex);
    if (!m_dirtyChunks.isEmpty()) {
        // Abort any rendering
        abortRendering();
        m_waitingThumbs.clear();
        // clear log
        m_errorLog.clear();
        const QString sceneList = m_cacheDir.absoluteFilePath(QStringLiteral("preview.mlt"));
        if (!KdenliveSettings::proxypreview() && pCore->currentDoc()->useProxy()) {
            const QString playlist =
                pCore->projectItemModel()->sceneList(m_cacheDir.absolutePath(), QString(), QString(), pCore->currentDoc()->getTimeline(m_uuid)->tractor(), -1);
            QDomDocument doc;
            doc.setContent(playlist);
            KdenliveDoc::useOriginals(doc);
            if (!Xml::docContentToFile(doc, sceneList)) {
                return;
            }
        } else {
            pCore->currentDoc()->getTimeline(m_uuid)->sceneList(m_cacheDir.absolutePath(), sceneList);
        }
        m_previewTimer.stop();
        doPreviewRender(sceneList);
    }
}

void PreviewManager::receivedStderr()
{
    QStringList resultList = QString::fromLocal8Bit(m_previewProcess.readAllStandardError()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (auto &result : resultList) {
        if (result.startsWith(QLatin1String("START:"))) {
            if (m_previewProcess.state() == QProcess::Running) {
                workingPreview = result.section(QLatin1String("START:"), 1).simplified().toInt();
                Q_EMIT workingPreviewChanged();
            }
        } else if (result.startsWith(QLatin1String("DONE:"))) {
            int chunk = result.section(QLatin1String("DONE:"), 1).simplified().toInt();
            m_processedChunks++;
            QString fileName = QStringLiteral("%1.%2").arg(chunk).arg(m_extension);
            Q_EMIT previewRender(chunk, m_cacheDir.absoluteFilePath(fileName), 1000 * m_processedChunks / m_chunksToRender);
        } else {
            m_errorLog.append(result);
        }
    }
}

void PreviewManager::doPreviewRender(const QString &scene)
{
    // initialize progress bar
    if (m_dirtyChunks.isEmpty()) {
        return;
    }
    QMutexLocker lock(&m_dirtyMutex);
    Q_ASSERT(m_previewProcess.state() == QProcess::NotRunning);
    std::sort(m_dirtyChunks.begin(), m_dirtyChunks.end(), chunkSort);
    const QStringList dirtyChunks = getCompressedList(m_dirtyChunks);
    m_chunksToRender = m_dirtyChunks.count();
    m_processedChunks = 0;
    int chunkSize = KdenliveSettings::timelinechunks();
    QStringList args{QStringLiteral("preview-chunks"),
                     scene,
                     m_cacheDir.absolutePath(),
                     dirtyChunks.join(QLatin1Char(',')),
                     QString::number(chunkSize - 1),
                     pCore->getCurrentProfilePath(),
                     m_extension,
                     m_consumerParams.join(QLatin1Char(' '))};
    pCore->currentDoc()->previewProgress(0);
    m_previewProcess.start(KdenliveSettings::kdenliverendererpath(), args);
    if (m_previewProcess.waitForStarted()) {
        qDebug() << " -  - -STARTING PREVIEW JOBS . . . STARTED: " << args;
    }
}

void PreviewManager::processEnded(int exitCode, QProcess::ExitStatus status)
{
    const QString sceneList = m_cacheDir.absoluteFilePath(QStringLiteral("preview.mlt"));
    QFile::remove(sceneList);
    if (pCore->window() && (status == QProcess::QProcess::CrashExit || exitCode != 0)) {
        Q_EMIT previewRender(0, m_errorLog, -1);
        if (workingPreview >= 0) {
            const QString fileName = QStringLiteral("%1.%2").arg(workingPreview).arg(m_extension);
            if (m_cacheDir.exists(fileName)) {
                m_cacheDir.remove(fileName);
            }
        }
    } else {
        // Normal exit and exit code 0: everything okay
        pCore->currentDoc()->previewProgress(1000);
    }
    workingPreview = -1;
    m_warnOnCrash = true;
    Q_EMIT workingPreviewChanged();
}

void PreviewManager::slotProcessDirtyChunks()
{
    if (m_dirtyChunks.isEmpty()) {
        return;
    }
    invalidatePreviews();
    if (KdenliveSettings::autopreview()) {
        m_previewTimer.start();
    }
}

void PreviewManager::slotRemoveInvalidUndo(int ix)
{
    QMutexLocker lock(&m_previewMutex);
    if (m_undoDir.dirName() != QLatin1String("undo")) {
        // Make sure we delete correct folder
        return;
    }
    QStringList dirs = m_undoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool ok;
    for (const QString &dir : qAsConst(dirs)) {
        if (dir.toInt(&ok) >= ix && ok) {
            QDir tmp = m_undoDir;
            if (tmp.cd(dir)) {
                tmp.removeRecursively();
            }
        }
    }
}

void PreviewManager::invalidatePreview(int startFrame, int endFrame)
{
    if (m_previewTrack == nullptr) {
        return;
    }
    int chunkSize = KdenliveSettings::timelinechunks();
    int start = startFrame - startFrame % chunkSize;
    int end = endFrame - endFrame % chunkSize;

    m_previewGatherTimer.stop();
    bool previewWasRunning = m_previewProcess.state() == QProcess::Running;
    bool alreadyRendered = false;
    bool wasInDirtyZone = false;
    if (!m_renderedChunks.isEmpty()) {
        // Check if the invalidated zone was already rendered
        std::sort(m_renderedChunks.begin(), m_renderedChunks.end(), chunkSort);
        if (start <= m_renderedChunks.last().toInt() && end >= m_renderedChunks.first().toInt()) {
            alreadyRendered = true;
        } else if (workingPreview >= start && workingPreview <= end) {
            alreadyRendered = true;
        }
    }
    if (!alreadyRendered && !m_dirtyChunks.isEmpty()) {
        // Check if the invalidate zone is in the current todo list (dirtychunks)
        std::sort(m_dirtyChunks.begin(), m_dirtyChunks.end(), chunkSort);
        if (start <= m_dirtyChunks.last().toInt() && end >= m_dirtyChunks.first().toInt()) {
            wasInDirtyZone = true;
        }
    }
    if (alreadyRendered) {
        if (previewWasRunning) {
            abortRendering();
        }
        m_tractor->lock();
        bool chunksChanged = false;
        for (int i = start; i <= end; i += chunkSize) {
            if (m_renderedChunks.contains(i)) {
                int ix = m_previewTrack->get_clip_index_at(i);
                if (m_previewTrack->is_blank(ix)) {
                    continue;
                }
                Mlt::Producer *prod = m_previewTrack->replace_with_blank(ix);
                delete prod;
                QVariant val(i);
                m_renderedChunks.removeAll(val);
                if (!m_dirtyChunks.contains(val)) {
                    QMutexLocker lock(&m_dirtyMutex);
                    m_dirtyChunks << val;
                    chunksChanged = true;
                }
            }
        }
        m_tractor->unlock();
        if (chunksChanged) {
            m_previewTrack->consolidate_blanks();
            Q_EMIT renderedChunksChanged();
            Q_EMIT dirtyChunksChanged();
        }
    } else if (wasInDirtyZone) {
        // Abort rendering, playlist needs to be recreated
        if (previewWasRunning) {
            abortRendering();
        }
    } else {
        // Invalidated zone outside our rendered zones
        return;
    }
    m_previewGatherTimer.start();
}

void PreviewManager::reloadChunks(const QVariantList &chunks)
{
    if (m_previewTrack == nullptr || chunks.isEmpty()) {
        return;
    }
    m_tractor->lock();
    for (const auto &ix : chunks) {
        if (m_previewTrack->is_blank_at(ix.toInt())) {
            QString fileName = m_cacheDir.absoluteFilePath(QStringLiteral("%1.%2").arg(ix.toInt()).arg(m_extension));
            fileName.prepend(QStringLiteral("avformat:"));
            Mlt::Producer prod(pCore->getProjectProfile(), fileName.toUtf8().constData());
            if (prod.is_valid()) {
                // m_ruler->updatePreview(ix, true);
                prod.set("mlt_service", "avformat-novalidate");
                m_previewTrack->insert_at(ix.toInt(), &prod, 1);
            }
        }
    }
    m_previewTrack->consolidate_blanks();
    m_tractor->unlock();
}

void PreviewManager::gotPreviewRender(int frame, const QString &file, int progress)
{
    if (m_previewTrack == nullptr) {
        return;
    }
    if (frame < 0) {
        pCore->currentDoc()->previewProgress(1000);
        return;
    }
    if (file.isEmpty() || progress < 0) {
        pCore->currentDoc()->previewProgress(progress);
        if (progress < 0) {
            if (m_warnOnCrash) {
                pCore->displayMessage(i18n("Preview rendering failed, check your parameters. %1Show details...%2",
                                           QString("<a href=\"" + QString::fromLatin1(QUrl::toPercentEncoding(file)) + QStringLiteral("\">")),
                                           QStringLiteral("</a>")),
                                      MltError);
            } else {
                // TODO display info about stopped preview job
            }
        }
        return;
    }
    if (m_previewTrack->is_blank_at(frame)) {
        Mlt::Producer prod(pCore->getProjectProfile(), QString("avformat:%1").arg(file).toUtf8().constData());
        if (prod.is_valid() && prod.get_length() == KdenliveSettings::timelinechunks()) {
            m_dirtyMutex.lock();
            m_dirtyChunks.removeAll(QVariant(frame));
            m_dirtyMutex.unlock();
            m_renderedChunks << frame;
            Q_EMIT renderedChunksChanged();
            prod.set("mlt_service", "avformat-novalidate");
            m_tractor->lock();
            m_previewTrack->insert_at(frame, &prod, 1);
            m_previewTrack->consolidate_blanks();
            m_tractor->unlock();
            pCore->currentDoc()->previewProgress(progress);
            pCore->currentDoc()->setModified(true);
        } else {
            qCDebug(KDENLIVE_LOG) << "* * * INVALID PROD: " << file;
            corruptedChunk(frame, file);
        }
    } else {
        qCDebug(KDENLIVE_LOG) << "* * * NON EMPTY PROD: " << frame;
    }
}

void PreviewManager::corruptedChunk(int frame, const QString &fileName)
{
    Q_EMIT abortPreview();
    m_previewProcess.waitForFinished();
    if (workingPreview >= 0) {
        workingPreview = -1;
        Q_EMIT workingPreviewChanged();
    }
    Q_EMIT previewRender(0, m_errorLog, -1);
    m_cacheDir.remove(fileName);
    if (!m_dirtyChunks.contains(frame)) {
        QMutexLocker lock(&m_dirtyMutex);
        m_dirtyChunks << frame;
        std::sort(m_dirtyChunks.begin(), m_dirtyChunks.end(), chunkSort);
    }
}

int PreviewManager::setOverlayTrack(Mlt::Playlist *overlay)
{
    m_overlayTrack = overlay;
    m_overlayTrack->set("kdenlive:playlistid", "timeline_overlay");
    reconnectTrack();
    return m_previewTrackIndex;
}

void PreviewManager::removeOverlayTrack()
{
    delete m_overlayTrack;
    m_overlayTrack = nullptr;
    reconnectTrack();
}

QPair<QStringList, QStringList> PreviewManager::previewChunks()
{
    QMutexLocker lock(&m_dirtyMutex);
    std::sort(m_renderedChunks.begin(), m_renderedChunks.end(), chunkSort);
    const QStringList renderedChunks = getCompressedList(m_renderedChunks);
    std::sort(m_dirtyChunks.begin(), m_dirtyChunks.end(), chunkSort);
    const QStringList dirtyChunks = getCompressedList(m_dirtyChunks);
    lock.unlock();
    return {renderedChunks, dirtyChunks};
}

const QStringList PreviewManager::getCompressedList(const QVariantList items) const
{
    QStringList resultString;
    int lastFrame = -1;
    QString currentString;
    for (const QVariant &frame : items) {
        int current = frame.toInt();
        if (current - KdenliveSettings::timelinechunks() == lastFrame) {
            lastFrame = current;
            if (frame == items.last()) {
                currentString.append(QString("-%1").arg(lastFrame));
                resultString << currentString;
                currentString.clear();
            }
            continue;
        }
        if (currentString.isEmpty()) {
            currentString = frame.toString();
        } else if (currentString == QString::number(lastFrame)) {
            // Only one chunk, store it
            resultString << currentString;
            currentString = frame.toString();
        } else {
            // Range, store
            currentString.append(QString("-%1").arg(lastFrame));
            resultString << currentString;
            currentString = frame.toString();
        }
        lastFrame = current;
    }
    if (!currentString.isEmpty()) {
        resultString << currentString;
    }
    return resultString;
}

bool PreviewManager::hasOverlayTrack() const
{
    return m_overlayTrack != nullptr;
}

bool PreviewManager::hasPreviewTrack() const
{
    return m_previewTrack != nullptr;
}

int PreviewManager::addedTracks() const
{
    if (m_previewTrack) {
        if (m_overlayTrack) {
            return 2;
        }
        return 1;
    } else if (m_overlayTrack) {
        return 1;
    }
    return -1;
}

bool PreviewManager::isRunning() const
{
    return workingPreview >= 0 || m_previewProcess.state() != QProcess::NotRunning;
}
