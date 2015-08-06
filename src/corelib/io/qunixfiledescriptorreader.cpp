/****************************************************************************
**
** Copyright (C) 2016 Intel Corporation.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#include "qunixfiledescriptorreader_p.h"
#include <qcoreevent.h>
#include <private/qcore_unix_p.h>
#include <private/qsocketnotifier_p.h>

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

class QUnixFileDescriptorReaderPrivate : public QSocketNotifierPrivate {};

QUnixFileDescriptorReader::QUnixFileDescriptorReader(int fd, QObject *parent)
    : QSocketNotifier(fd, Read, parent),
      readBufferMaxSize(-1)
{
}

QUnixFileDescriptorReader::~QUnixFileDescriptorReader()
{
    qt_safe_close(fileDescriptor());
}

int QUnixFileDescriptorReader::fileDescriptor() const
{
    return d_func()->sockfd;
}

void QUnixFileDescriptorReader::close()
{
    setEnabled(false);
    qt_safe_close(d_func()->sockfd);
    d_func()->sockfd = -1;
}

qint64 QUnixFileDescriptorReader::read(char *data, qint64 maxlen)
{
    if (buffer.isEmpty())
        return -2;              // simulate EWOULDBLOCK
    int n = buffer.read(data, maxlen);
    setEnabled(uint(buffer.size()) >= uint(readBufferMaxSize));
    return n;
}

bool QUnixFileDescriptorReader::waitForReadyRead(int msecs)
{
    Q_ASSERT(msecs == 0);       // we don't actually support waiting

    return doRead() != -1;
}

bool QUnixFileDescriptorReader::waitForClosed(int msecs)
{
    Q_ASSERT(msecs == 0);       // we don't actually support waiting

    return doRead() != -1 && fileDescriptor() != -1;
}

bool QUnixFileDescriptorReader::event(QEvent *e)
{
    bool parentresult = QSocketNotifier::event(e);
    if (e->type() == QEvent::SockAct || e->type() == QEvent::SockClose)
        doRead();
    return parentresult;
}

qint64 QUnixFileDescriptorReader::doRead()
{
    int fd = fileDescriptor();
    Q_ASSERT(fd >= 0);

    int nbytes = 0;
    if (::ioctl(fd, FIONREAD, (char *) &nbytes) < 0)
        nbytes = 1;             // always try to read at least one byte

    nbytes = qBound<uint>(1, nbytes, readBufferMaxSize - buffer.size());

    char *ptr = buffer.reserve(nbytes);
    qint64 nread = qt_safe_read(fd, ptr, nbytes);
    buffer.chop(nbytes - qMax<int>(nread, 0));

    if (nread > 0) {
        setEnabled(uint(buffer.size()) >= uint(readBufferMaxSize));
        emit readyRead(QPrivateSignal());
    } else if (nread == -1 && errno == EWOULDBLOCK) {
        return -2;
    } else {
        // error reading or EOF, don't try again
        close();
        emit closed(QPrivateSignal());
    }

    return nread;
}

QT_END_NAMESPACE
