/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qelapsedtimer.h>
#include <qcoreapplication.h>

#include "private/qcore_unix_p.h"
#include "private/qtimerinfo_unix_p.h"
#include "private/qobject_p.h"
#include "private/qabstracteventdispatcher_p.h"

#include <sys/times.h>

QT_BEGIN_NAMESPACE

Q_CORE_EXPORT bool qt_disable_lowpriority_timers=false;

/*
 * Internal functions for manipulating timer data structures.  The
 * timerBitVec array is used for keeping track of timer identifiers.
 */

QTimerInfoList::QTimerInfoList()
{
    firstTimerInfo = 0;
}

QDeadlineTimer QTimerInfoList::currentTime()
{
    return QDeadlineTimer::current(Qt::PreciseTimer);
}

/*
  insert timer info into list
*/
void QTimerInfoList::timerInsert(QTimerInfo *ti)
{
    int index = size();
    while (index--) {
        const QTimerInfo * const t = at(index);
        if (!(ti->deadline < t->deadline))
            break;
    }
    insert(index+1, ti);
}

inline timespec &operator+=(timespec &t1, int ms)
{
    t1.tv_sec += ms / 1000;
    t1.tv_nsec += ms % 1000 * 1000 * 1000;
    return normalizedTimespec(t1);
}

inline timespec operator+(const timespec &t1, int ms)
{
    timespec t2 = t1;
    return t2 += ms;
}

static void calculateCoarseTimerTimeout(QTimerInfo *t, QDeadlineTimer currentTime)
{
    uint interval = uint(t->interval);
    qint64 timeout = t->deadline.deadline();
    uint msec = timeout  % 1000;
    timeout -= msec;
    Q_ASSERT(interval >= 20);

    msec = QAbstractEventDispatcherPrivate::calculateCoarseTimerBoundary(interval, msec);

    t->deadline.setDeadline(timeout + msec, t->deadline.timerType());

    if (t->deadline < currentTime)
        t->deadline += interval;
}

static void calculateNextTimeout(QTimerInfo *t, QDeadlineTimer currentTime)
{
    switch (t->timerType) {
    case Qt::PreciseTimer:
    case Qt::CoarseTimer:
        t->deadline += t->interval;
        if (t->deadline < currentTime)
            t->deadline = currentTime + t->interval;
        if (t->timerType == Qt::CoarseTimer)
            calculateCoarseTimerTimeout(t, currentTime);
        return;

    case Qt::VeryCoarseTimer:
        // we don't need to take care of the microsecond component of t->interval
        t->deadline += t->interval;
        qint64 secs = t->deadline.deadline() / 1000;
        if (secs <= currentTime.deadline() / 1000)
            t->deadline = currentTime + t->interval;
        return;
    }
}

/*
  Returns the time to wait for the next timer, or QDeadlineTimer::Forever if no
  timers are waiting.
*/
QDeadlineTimer QTimerInfoList::nextDeadline() const
{
    // Find first waiting timer not already active
    QTimerInfo *t = 0;
    for (QTimerInfoList::const_iterator it = constBegin(); it != constEnd(); ++it) {
        if (!(*it)->activateRef) {
            t = *it;
            break;
        }
    }

    if (!t)
        return QDeadlineTimer::Forever;
    return t->deadline;
}

/*
  Returns the timer's remaining time in milliseconds with the given timerId, or
  null if there is nothing left. If the timer id is not found in the list, the
  returned value will be -1. If the timer is overdue, the returned value will be 0.
*/
int QTimerInfoList::timerRemainingTime(int timerId)
{
    for (int i = 0; i < count(); ++i) {
        QTimerInfo *t = at(i);
        if (t->id == timerId)
            return t->deadline.remainingTime();
    }

#ifndef QT_NO_DEBUG
    qWarning("QTimerInfoList::timerRemainingTime: timer id %i not found", timerId);
#endif

    return -1;
}

void QTimerInfoList::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object)
{
    QTimerInfo *t = new QTimerInfo;
    t->id = timerId;
    t->interval = interval;
    t->timerType = timerType;
    t->obj = object;
    t->activateRef = 0;

    switch (timerType) {
    case Qt::PreciseTimer:
        // high precision timer is based on millisecond precision
        // so no adjustment is necessary
        t->deadline.setRemainingTime(interval, Qt::PreciseTimer);
        break;

    case Qt::CoarseTimer:
        // this timer has up to 5% coarseness
        // so our boundaries are 20 ms and 20 s
        // below 20 ms, 5% inaccuracy is below 1 ms, so we convert to high precision
        // above 20 s, 5% inaccuracy is above 1 s, so we convert to VeryCoarseTimer
        if (interval < 20000) {
            // note: despite the timer type, the QDeadlineTimer is always using Qt::PreciseTimer
            QDeadlineTimer now = QDeadlineTimer::current(Qt::PreciseTimer);
            t->deadline = now + interval;
            if (interval <= 20) {
                t->timerType = Qt::PreciseTimer;
                // no adjustment is necessary
            } else {
                calculateCoarseTimerTimeout(t, now);
            }
            break;
        }
        Q_FALLTHROUGH();
    case Qt::VeryCoarseTimer:
        // the very coarse timer is based on full second precision
        t->interval += 499;
        t->interval /= 1000;
        t->interval *= 1000;

        // if we're past the half-second mark, increase the timeout again
        auto now = QDeadlineTimer::current(Qt::VeryCoarseTimer);
        int x = now.deadline() % 1000;
        t->deadline = now + (interval + (x > 500 ? 1000 : 0) - x);
    }

    timerInsert(t);

#ifdef QTIMERINFO_DEBUG
    t->expected = expected;
    t->cumulativeError = 0;
    t->count = 0;
    if (t->timerType != Qt::PreciseTimer)
    qDebug() << "timer" << t->timerType << hex <<t->id << dec << "interval" << t->interval << "expected at"
            << t->expected << "will fire first at" << t->timeout;
#endif
}

bool QTimerInfoList::unregisterTimer(int timerId)
{
    // set timer inactive
    for (int i = 0; i < count(); ++i) {
        QTimerInfo *t = at(i);
        if (t->id == timerId) {
            // found it
            removeAt(i);
            if (t == firstTimerInfo)
                firstTimerInfo = 0;
            if (t->activateRef)
                *(t->activateRef) = 0;
            delete t;
            return true;
        }
    }
    // id not found
    return false;
}

bool QTimerInfoList::unregisterTimers(QObject *object)
{
    if (isEmpty())
        return false;
    for (int i = 0; i < count(); ++i) {
        QTimerInfo *t = at(i);
        if (t->obj == object) {
            // object found
            removeAt(i);
            if (t == firstTimerInfo)
                firstTimerInfo = 0;
            if (t->activateRef)
                *(t->activateRef) = 0;
            delete t;
            // move back one so that we don't skip the new current item
            --i;
        }
    }
    return true;
}

QList<QAbstractEventDispatcher::TimerInfo> QTimerInfoList::registeredTimers(QObject *object) const
{
    QList<QAbstractEventDispatcher::TimerInfo> list;
    for (int i = 0; i < count(); ++i) {
        const QTimerInfo * const t = at(i);
        if (t->obj == object) {
            list << QAbstractEventDispatcher::TimerInfo(t->id,
                                                        t->interval,
                                                        t->timerType);
        }
    }
    return list;
}

/*
    Activate pending timers, returning how many where activated.
*/
int QTimerInfoList::activateTimers()
{
    if (qt_disable_lowpriority_timers || isEmpty())
        return 0; // nothing to do

    int n_act = 0, maxCount = 0;
    firstTimerInfo = 0;

    QDeadlineTimer currentTime = this->currentTime();
    // qDebug() << "Thread" << QThread::currentThreadId() << "woken up at" << currentTime;

    // Find out how many timer have expired
    for (QTimerInfoList::const_iterator it = constBegin(); it != constEnd(); ++it) {
        if (currentTime < (*it)->deadline)
            break;
        maxCount++;
    }

    //fire the timers.
    while (maxCount--) {
        if (isEmpty())
            break;

        QTimerInfo *currentTimerInfo = constFirst();
        if (currentTime < currentTimerInfo->deadline)
            break; // no timer has expired

        if (!firstTimerInfo) {
            firstTimerInfo = currentTimerInfo;
        } else if (firstTimerInfo == currentTimerInfo) {
            // avoid sending the same timer multiple times
            break;
        } else if (currentTimerInfo->interval <  firstTimerInfo->interval
                   || currentTimerInfo->interval == firstTimerInfo->interval) {
            firstTimerInfo = currentTimerInfo;
        }

        // remove from list
        removeFirst();

        // determine next timeout time
        calculateNextTimeout(currentTimerInfo, currentTime);

        // reinsert timer
        timerInsert(currentTimerInfo);
        if (currentTimerInfo->interval > 0)
            n_act++;

        if (!currentTimerInfo->activateRef) {
            // send event, but don't allow it to recurse
            currentTimerInfo->activateRef = &currentTimerInfo;

            QTimerEvent e(currentTimerInfo->id);
            QCoreApplication::sendEvent(currentTimerInfo->obj, &e);

            if (currentTimerInfo)
                currentTimerInfo->activateRef = 0;
        }
    }

    firstTimerInfo = 0;
    // qDebug() << "Thread" << QThread::currentThreadId() << "activated" << n_act << "timers";
    return n_act;
}

QT_END_NAMESPACE
