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

#include "qunixfiledescriptorwriter_p.h"
#include <qcoreevent.h>
#include <private/qcore_unix_p.h>
#include <private/qsocketnotifier_p.h>

#include <errno.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

class QUnixFileDescriptorWriterPrivate : public QSocketNotifierPrivate {};

QUnixFileDescriptorWriter::QUnixFileDescriptorWriter(int fd, QObject *parent)
    : QSocketNotifier(fd, Write, Disabled, parent)
{
}

QUnixFileDescriptorWriter::~QUnixFileDescriptorWriter()
{
    qt_safe_close(fileDescriptor());
}

int QUnixFileDescriptorWriter::fileDescriptor() const
{
    return d_func()->sockfd;
}

void QUnixFileDescriptorWriter::close()
{
    setEnabled(false);
    qt_safe_close(d_func()->sockfd);
    d_func()->sockfd = -1;
}

qint64 QUnixFileDescriptorWriter::write(const char *data, qint64 len)
{
    buffer.append(QByteArray(data, len));
    setEnabled(!buffer.isEmpty());
    return len;
}

bool QUnixFileDescriptorWriter::waitForBytesWritten(int msecs)
{
    Q_ASSERT(msecs == 0);                       // we don't support waiting
    return doWrite() >= 0;
}

bool QUnixFileDescriptorWriter::event(QEvent *e)
{
    bool parentresult = QSocketNotifier::event(e);
    if (e->type() == QEvent::SockAct || e->type() == QEvent::SockClose)
        doWrite();
    return parentresult;
}

qint64 QUnixFileDescriptorWriter::doWrite()
{
    int fd = fileDescriptor();
    Q_ASSERT(fd >= 0);

    int towrite = buffer.nextDataBlockSize();
    const char *ptr = buffer.readPointer();
    if (!ptr)
        return 0;

    qint64 written = qt_safe_write_nosignal(fd, ptr, towrite);
    if (written > 0) {
        buffer.free(written);
        setEnabled(!buffer.isEmpty());
        emit bytesWritten(written);
    } else if (written < 0 && errno == EWOULDBLOCK) {
        return 0;
    } else if (written < 0) {
        close();
    }

    return written;
}

QT_END_NAMESPACE
