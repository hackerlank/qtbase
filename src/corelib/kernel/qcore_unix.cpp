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

#include "qcore_unix_p.h"
#include "qelapsedtimer.h"

#include <stdlib.h>

#ifdef Q_OS_MAC
#include <mach/mach_time.h>
#endif

QT_BEGIN_NAMESPACE

namespace QtSigpipeStatus {
QBasicAtomicInt status  = Q_BASIC_ATOMIC_INITIALIZER(0);
}

#if !defined(QT_HAVE_PPOLL) && defined(QT_HAVE_POLLTS)
# define ppoll pollts
# define QT_HAVE_PPOLL
#endif

// defined in qpoll.cpp
int qt_poll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts);

static inline int qt_ppoll(struct pollfd *fds, nfds_t nfds, QDeadlineTimer deadline)
{
#if defined(QT_HAVE_PPOLL) || !defined(QT_HAVE_POLL)
    qint64 nsec = deadline.remainingTimeNSecs();
    struct timespec *pts = nullptr;
    struct timespec ts;
    if (nsec != -1) {
        pts = &ts;
        ts.tv_sec = nsec / (1000 * 1000 * 1000);
        ts.tv_nsec = (nsec - ts.tv_sec * 1000 * 1000 * 1000);
    }

#   ifdef QT_HAVE_PPOLL
    return ::ppoll(fds, nfds, pts, nullptr);
#  else
    return qt_poll(fds, nfds, pts);
#  endif
#else
    return ::poll(fds, nfds, deadline.remainingTime());
#endif
}


/*!
    \internal

    Behaves as close to POSIX poll(2) as practical but may be implemented
    using select(2) where necessary. In that case, returns -1 and sets errno
    to EINVAL if passed any descriptor greater than or equal to FD_SETSIZE.
*/
int qt_safe_poll(struct pollfd *fds, nfds_t nfds, QDeadlineTimer deadline)
{
    int ret;
    EINTR_LOOP(ret, qt_ppoll(fds, nfds, deadline));
    return ret;
}

QT_END_NAMESPACE
