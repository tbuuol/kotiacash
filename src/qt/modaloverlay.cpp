// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "modaloverlay.h"
#include "ui_modaloverlay.h"

#include "guiutil.h"

#include <QResizeEvent>
#include <QPropertyAnimation>

ModalOverlay::ModalOverlay(QWidget *parent) :
QWidget(parent),
ui(new Ui::ModalOverlay),
bestHeaderHeight(0),
bestHeaderDate(QDateTime()),
layerIsVisible(false),
userClosed(false)
{
    ui->setupUi(this);
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(closeClicked()));
    if (parent) {
        parent->installEventFilter(this);
        raise();
    }

    blockProcessTime.clear();
    blockHeightTime.clear();
    setVisible(false);
}

ModalOverlay::~ModalOverlay()
{
    delete ui;
}

bool ModalOverlay::eventFilter(QObject * obj, QEvent * ev) {
    if (obj == parent()) {
        if (ev->type() == QEvent::Resize) {
            QResizeEvent * rev = static_cast<QResizeEvent*>(ev);
            resize(rev->size());
            if (!layerIsVisible)
                setGeometry(0, height(), width(), height());

        }
        else if (ev->type() == QEvent::ChildAdded) {
            raise();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

//! Tracks parent widget changes
bool ModalOverlay::event(QEvent* ev) {
    if (ev->type() == QEvent::ParentAboutToChange) {
        if (parent()) parent()->removeEventFilter(this);
    }
    else if (ev->type() == QEvent::ParentChange) {
        if (parent()) {
            parent()->installEventFilter(this);
            raise();
        }
    }
    return QWidget::event(ev);
}

void ModalOverlay::setKnownBestHeight(int count, const QDateTime& blockDate)
{
    if (count > bestHeaderHeight) {
        bestHeaderHeight = count;
        bestHeaderDate = blockDate;
    }
}

void ModalOverlay::tipUpdate(int count, const QDateTime& blockDate, double nVerificationProgress)
{
    QDateTime currentDate = QDateTime::currentDateTime();
    int blocksLeft = 0;
    if (bestHeaderHeight > count)
        blocksLeft = bestHeaderHeight - count;

    double displayProgress = nVerificationProgress;
    if (displayProgress != displayProgress || displayProgress < 0.0)
        displayProgress = 0.0;
    if (displayProgress > 1.0)
        displayProgress = 1.0;

    // Kotia's checkpoint tx/day metadata is incomplete, so work-based progress
    // can report 100% while headers prove that sync is still far behind.
    if (blocksLeft > 0 && bestHeaderHeight > 0) {
        double heightProgress = (double)count / (double)bestHeaderHeight;
        if (heightProgress < 0.0)
            heightProgress = 0.0;
        if (heightProgress > 0.9999)
            heightProgress = 0.9999;
        if (displayProgress >= 0.9999 || displayProgress < heightProgress)
            displayProgress = heightProgress;
    }

    // keep a vector of samples of verification progress at height
    blockProcessTime.push_front(qMakePair(currentDate.toMSecsSinceEpoch(), displayProgress));
    blockHeightTime.push_front(qMakePair(currentDate.toMSecsSinceEpoch(), count));

    // show progress speed if we have more then one sample
    if (blockProcessTime.size() >= 2)
    {
        double progressDelta = 0;
        qint64 timeDelta = 0;
        qint64 remainingMSecs = 0;
        double remainingProgress = 1.0 - displayProgress;

        // Use the oldest sample that's at least 30 seconds old, or the oldest available
        static const qint64 MIN_SAMPLE_AGE_MS = 30 * 1000;
        static const qint64 MAX_SAMPLE_AGE_MS = 600 * 1000;
        for (int i = 1; i < blockProcessTime.size(); i++)
        {
            QPair<qint64, double> sample = blockProcessTime[i];
            qint64 age = currentDate.toMSecsSinceEpoch() - sample.first;

            if (age >= MIN_SAMPLE_AGE_MS || i == blockProcessTime.size() - 1) {
                progressDelta = blockProcessTime[0].second - sample.second;
                timeDelta = blockProcessTime[0].first - sample.first;
                if (timeDelta <= 0) {
                    timeDelta = 1000;
                }
                break;
            }
            if (age >= MAX_SAMPLE_AGE_MS) {
                progressDelta = blockProcessTime[0].second - sample.second;
                timeDelta = blockProcessTime[0].first - sample.first;
                if (timeDelta <= 0) {
                    timeDelta = 1000;
                }
                break;
            }
        }

        // Calculate progress per hour and remaining time with sanity checks
        double progressPerHour = 0;
        if (progressDelta > 0 && timeDelta > 0) {
            progressPerHour = progressDelta / (double)timeDelta * 1000 * 3600;
            remainingMSecs = (qint64)(remainingProgress / progressDelta * timeDelta);
            // Cap remaining time to 30 days max to avoid absurd estimates
            static const qint64 MAX_REMAINING_MS = 30LL * 24 * 3600 * 1000;
            if (remainingMSecs > MAX_REMAINING_MS) {
                remainingMSecs = MAX_REMAINING_MS;
            }
            if (remainingMSecs < 0) {
                remainingMSecs = 0;
            }
        } else {
            // No work progress yet: estimate from observed block height speed.
            if (blocksLeft > 0) {
                for (int i = 1; i < blockHeightTime.size(); i++) {
                    QPair<qint64, int> sample = blockHeightTime[i];
                    int blocksDone = blockHeightTime[0].second - sample.second;
                    qint64 blockTimeDelta = blockHeightTime[0].first - sample.first;
                    if (blocksDone > 0 && blockTimeDelta > 0) {
                        remainingMSecs = (qint64)((double)blocksLeft / (double)blocksDone * (double)blockTimeDelta);
                        break;
                    }
                }

                // Fallback to target spacing if the wallet has not advanced yet.
                if (remainingMSecs <= 0)
                    remainingMSecs = blocksLeft * 120LL * 1000LL;

                static const qint64 MAX_REMAINING_MS = 30LL * 24 * 3600 * 1000;
                if (remainingMSecs > MAX_REMAINING_MS) remainingMSecs = MAX_REMAINING_MS;
            }
        }

        // show progress increase per hour
        ui->progressIncreasePerH->setText(QString::number(progressPerHour * 100, 'f', 2) + "%");

        // show expected remaining time
        ui->expectedTimeLeft->setText(GUIUtil::formateNiceTimeOffset(remainingMSecs / 1000.0));

        static const int MAX_SAMPLES = 5000;
        if (blockProcessTime.count() > MAX_SAMPLES)
            blockProcessTime.remove(MAX_SAMPLES, blockProcessTime.count() - MAX_SAMPLES);
        if (blockHeightTime.count() > MAX_SAMPLES)
            blockHeightTime.remove(MAX_SAMPLES, blockHeightTime.count() - MAX_SAMPLES);
    }

    // show the last block date
    ui->newestBlockDate->setText(blockDate.toString());

    // show the percentage done according to nVerificationProgress
    ui->percentageProgress->setText(QString::number(displayProgress * 100, 'f', 2) + "%");
    ui->progressBar->setMaximum(100000000);
    ui->progressBar->setValue(displayProgress * 100000000.0 + 0.5);

    if (!bestHeaderDate.isValid())
        // not syncing
        return;

    // estimate the number of headers left based on nPowTargetSpacing
    // and check if the gui is not aware of the the best header (happens rarely)
    int estimateNumHeadersLeft = bestHeaderDate.secsTo(currentDate) / 600;
    bool hasBestHeader = bestHeaderHeight >= count;

    // show remaining number of blocks
    if (estimateNumHeadersLeft < 24 && hasBestHeader) {
        ui->numberOfBlocksLeft->setText(QString::number(blocksLeft));
    } else {
        ui->numberOfBlocksLeft->setText(tr("Unknown. Syncing Headers (%1)...").arg(bestHeaderHeight));
        ui->expectedTimeLeft->setText(tr("Unknown..."));
    }
}

void ModalOverlay::showHide(bool hide, bool userRequested)
{
    if ( (layerIsVisible && !hide) || (!layerIsVisible && hide) || (!hide && userClosed && !userRequested))
        return;

    if (!isVisible() && !hide)
        setVisible(true);

    setGeometry(0, hide ? 0 : height(), width(), height());

    QPropertyAnimation* animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(300);
    animation->setStartValue(QPoint(0, hide ? 0 : this->height()));
    animation->setEndValue(QPoint(0, hide ? this->height() : 0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    layerIsVisible = !hide;
}

void ModalOverlay::closeClicked()
{
    showHide(true);
    userClosed = true;
}
