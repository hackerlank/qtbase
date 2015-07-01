/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QABSTRACTEVENTDISPATCHER_P_H
#define QABSTRACTEVENTDISPATCHER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "QtCore/qabstracteventdispatcher.h"
#include "private/qobject_p.h"

QT_BEGIN_NAMESPACE

Q_CORE_EXPORT uint qGlobalPostedEventsCount();

class Q_CORE_EXPORT QAbstractEventDispatcherPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QAbstractEventDispatcher)
public:
    inline QAbstractEventDispatcherPrivate()
    { }

    QList<QAbstractNativeEventFilter *> eventFilters;

    static int allocateTimerId();
    static void releaseTimerId(int id);

    static inline unsigned calculateCoarseTimerBoundary(uint interval, uint msec)
    {
        // The coarse timer works like this:
        //  - interval under 40 ms: round to even
        //  - between 40 and 99 ms: round to multiple of 4
        //  - otherwise: try to wake up at a multiple of 25 ms, with a maximum error of 5%
        //
        // We try to wake up at the following second-fraction, in order of preference:
        //    0 ms
        //  500 ms
        //  250 ms or 750 ms
        //  200, 400, 600, 800 ms
        //  other multiples of 100
        //  other multiples of 50
        //  other multiples of 25
        //
        // The objective is to make most timers wake up at the same time, thereby reducing CPU wakeups.

        // Calculate how much we can round and still keep within 5% error
        uint absMaxRounding = interval / 20;

        if (interval < 100 && interval != 25 && interval != 50 && interval != 75) {
            // special mode for timers of less than 100 ms
            if (interval < 50) {
                // round to even
                // round towards multiples of 50 ms
                bool roundUp = (msec % 50) >= 25;
                msec >>= 1;
                msec |= uint(roundUp);
                msec <<= 1;
            } else {
                // round to multiple of 4
                // round towards multiples of 100 ms
                bool roundUp = (msec % 100) >= 50;
                msec >>= 2;
                msec |= uint(roundUp);
                msec <<= 2;
            }
        } else {
            uint min = qMax<int>(0, msec - absMaxRounding);
            uint max = qMin(1000u, msec + absMaxRounding);

            // find the boundary that we want, according to the rules above
            // extra rules:
            // 1) whatever the interval, we'll take any round-to-the-second timeout
            if (min == 0) {
                return 0;
            } else if (max == 1000) {
                return 1000;
            }

            uint wantedBoundaryMultiple;

            // 2) if the interval is a multiple of 500 ms and > 5000 ms, we'll always round
            //    towards a round-to-the-second
            // 3) if the interval is a multiple of 500 ms, we'll round towards the nearest
            //    multiple of 500 ms
            if ((interval % 500) == 0) {
                if (interval >= 5000) {
                    return msec >= 500 ? max : min;
                } else {
                    wantedBoundaryMultiple = 500;
                }
            } else if ((interval % 50) == 0) {
                // 4) same for multiples of 250, 200, 100, 50
                uint mult50 = interval / 50;
                if ((mult50 % 4) == 0) {
                    // multiple of 200
                    wantedBoundaryMultiple = 200;
                } else if ((mult50 % 2) == 0) {
                    // multiple of 100
                    wantedBoundaryMultiple = 100;
                } else if ((mult50 % 5) == 0) {
                    // multiple of 250
                    wantedBoundaryMultiple = 250;
                } else {
                    // multiple of 50
                    wantedBoundaryMultiple = 50;
                }
            } else {
                wantedBoundaryMultiple = 25;
            }

            uint base = msec / wantedBoundaryMultiple * wantedBoundaryMultiple;
            uint middlepoint = base + wantedBoundaryMultiple / 2;
            if (msec < middlepoint)
                msec = qMax(base, min);
            else
                msec = qMin(base + wantedBoundaryMultiple, max);
        }

        return msec;
    }
};

QT_END_NAMESPACE

#endif // QABSTRACTEVENTDISPATCHER_P_H
