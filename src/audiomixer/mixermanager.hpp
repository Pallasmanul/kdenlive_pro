/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <memory>
#include <unordered_map>

#include <QSlider>
#include <QWidget>

namespace Mlt {
class Tractor;
}

class MixerWidget;
class QHBoxLayout;
class TimelineItemModel;
class QScrollArea;

/** MySlider class is only here to get the slider handle size used when painting the audio level scale */
class MySlider : public QSlider
{
public:
    explicit MySlider(QWidget *parent = nullptr);
    int getHandleHeight();
};

class MixerManager : public QWidget
{
    Q_OBJECT

public:
    MixerManager(QWidget *parent);
    /** @brief Shows the parameters of the given transition model */
    void registerTrack(int tid, Mlt::Tractor *service, const QString &trackTag, const QString &trackName);
    void deregisterTrack(int tid);
    void setModel(std::shared_ptr<TimelineItemModel> model);
    void cleanup();
    /** @brief Connect the mixer widgets to the correspondent filters */
    void connectMixer(bool doConnect);
    void collapseMixers();
    /** @brief Pause/unpause audio monitoring */
    void pauseMonitoring(bool pause);
    /** @brief Release the timeline model ownership */
    void unsetModel();
    /** @brief Some features rely on a specific version of MLT's audiolevel filter, so check it */
    void checkAudioLevelVersion();
    /** @brief Enable/disable audio monitoring on a track */
    void monitorAudio(int tid, bool monitor);
    /** @brief Track currently monitored that will be used for recording */
    int recordTrack() const;
    /** @brief Return true if we have MLT's audiolevel filter version 2 or above (fixes reading track audio level) */
    bool audioLevelV2() const;

public Q_SLOTS:
    void recordStateChanged(int tid, bool recording);

private Q_SLOTS:
    void resetSizePolicy();

Q_SIGNALS:
    void updateLevels(int);
    void purgeCache();
    void clearMixers();
    void updateRecVolume();
    void showEffectStack(int tid);

protected:
    std::unordered_map<int, std::shared_ptr<MixerWidget>> m_mixers;
    std::shared_ptr<MixerWidget> m_masterMixer;
    QSize sizeHint() const override;

private:
    std::shared_ptr<Mlt::Tractor> m_masterService;
    std::shared_ptr<TimelineItemModel> m_model;
    QHBoxLayout *m_box;
    QHBoxLayout *m_masterBox;
    QHBoxLayout *m_channelsLayout;
    QScrollArea *m_channelsBox;
    bool m_visibleMixerManager;
    int m_expandedWidth;
    QVector<int> m_soloMuted;
    int m_recommendedWidth;
    int m_monitorTrack;
    bool m_filterIsV2;
    int m_sliderHandle;
};
