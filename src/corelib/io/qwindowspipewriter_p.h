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

#ifndef QWINDOWSPIPEWRITER_P_H
#define QWINDOWSPIPEWRITER_P_H

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

#include <QtCore/private/qglobal_p.h>
#include <qdeadlinetimer.h>
#include <qobject.h>
#include <qbytearray.h>
#include <qt_windows.h>

QT_BEGIN_NAMESPACE

#define SLEEPMIN 10
#define SLEEPMAX 500

class QIncrementalSleepTimer
{

public:
    QIncrementalSleepTimer(QDeadlineTimer &deadline)
        : deadline(deadline),
          nextSleep(qMin<qint64>(SLEEPMIN, deadline.remainingTime()))
    {
        if (deadline.isForever())
            nextSleep = SLEEPMIN;
    }

    int nextSleepTime()
    {
        int tmp = nextSleep;
        nextSleep = qMin(nextSleep * 2, qMin(SLEEPMAX, timeLeft()));
        return tmp;
    }

    int timeLeft() const
    {
        qint64 left = deadline.remainingTime();
        if (quint64(left) > INT_MAX)    // catches -1 as well as overflow
            return SLEEPMAX;
        return int(left);
    }

    bool hasTimedOut() const
    {
        return deadline.hasExpired();
    }

    void resetIncrements()
    {
        nextSleep = qMin(SLEEPMIN, timeLeft());
    }

private:
    QDeadlineTimer &deadline;
    int nextSleep;
};

class Q_CORE_EXPORT QWindowsPipeWriter : public QObject
{
    Q_OBJECT
public:
    explicit QWindowsPipeWriter(HANDLE pipeWriteEnd, QObject *parent = 0);
    ~QWindowsPipeWriter();

    bool write(const QByteArray &ba);
    void stop();
    bool waitForWrite(QDeadlineTimer deadline);
    bool isWriteOperationActive() const { return writeSequenceStarted; }
    qint64 bytesToWrite() const;

Q_SIGNALS:
    void canWrite();
    void bytesWritten(qint64 bytes);
    void _q_queueBytesWritten(QPrivateSignal);

private:
    static void CALLBACK writeFileCompleted(DWORD errorCode, DWORD numberOfBytesTransfered,
                                            OVERLAPPED *overlappedBase);
    void notified(DWORD errorCode, DWORD numberOfBytesWritten);
    bool waitForNotification(QDeadlineTimer deadline);
    void emitPendingBytesWrittenValue();

    class Overlapped : public OVERLAPPED
    {
        Q_DISABLE_COPY(Overlapped)
    public:
        explicit Overlapped(QWindowsPipeWriter *pipeWriter);
        void clear();

        QWindowsPipeWriter *pipeWriter;
    };

    HANDLE handle;
    Overlapped overlapped;
    QByteArray buffer;
    qint64 numberOfBytesToWrite;
    qint64 pendingBytesWrittenValue;
    bool stopped;
    bool writeSequenceStarted;
    bool notifiedCalled;
    bool bytesWrittenPending;
    bool inBytesWritten;
};

QT_END_NAMESPACE

#endif // QT_NO_PROCESS
