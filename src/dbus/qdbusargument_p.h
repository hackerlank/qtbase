/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtDBus module of the Qt Toolkit.
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

#ifndef QDBUSARGUMENT_P_H
#define QDBUSARGUMENT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLibrary class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtDBus/private/qtdbusglobal_p.h>
#include <qdbusargument.h>
#include "qdbusmessage.h"
#include "qdbusunixfiledescriptor.h"
#include "qdbus_symbols_p.h"

#ifndef QT_NO_DBUS

#ifndef DBUS_TYPE_UNIX_FD
# define DBUS_TYPE_UNIX_FD int('h')
# define DBUS_TYPE_UNIX_FD_AS_STRING "h"
#endif

QT_BEGIN_NAMESPACE

enum { ExperimentalMarshallerDemarshaller = 0x80000000 };

class QDBusMarshaller;
class QDBusDemarshaller;
class QDBusArgumentPrivate
{
public:
    enum Direction {
        Marshalling,
        Demarshalling
    };
    QDBusArgumentPrivate(DBusMessage *msg, Direction dir, int flags = 0);
    virtual ~QDBusArgumentPrivate();

    static bool checkRead(QDBusArgumentPrivate *d);
    static bool checkReadAndDetach(QDBusArgumentPrivate *&d);
    static bool checkWrite(QDBusArgumentPrivate *&d);

    QDBusMarshaller *marshaller();
    QDBusDemarshaller *demarshaller();

    static QByteArray createSignature(int id);
    static inline QDBusArgument create(QDBusArgumentPrivate *d)
    {
        QDBusArgument q(d);
        return q;
    }
    static inline QDBusArgumentPrivate *d(QDBusArgument &q)
    { return q.d; }

public:
    DBusMessage *message;
    QAtomicInt ref;
    int capabilities;
    Direction direction;
};

#define NULLORLATIN1(str)   ((str).isEmpty() ? Q_NULLPTR : (str).toLatin1().constData())

class QDBusMarshaller: public QDBusArgumentPrivate
{
public:
    QDBusMarshaller(QDBusMessage::MessageType type, int flags);
    QDBusMarshaller(QByteArray *ba);
    ~QDBusMarshaller();

    QDBusMarshaller *detachedForWriting();

    void setDestinationService(const QString &service)
    { q_dbus_message_set_destination(message, NULLORLATIN1(service)); }
    void setPath(const QString &path)
    { q_dbus_message_set_path(message, NULLORLATIN1(path)); }
    void setInterface(const QString &interface)
    { q_dbus_message_set_interface(message, NULLORLATIN1(interface)); }
    void setErrorName(const QString &errorName)
    { q_dbus_message_set_error_name(message, NULLORLATIN1(errorName)); }
    void setMethodName(const QString &methodName)
    { q_dbus_message_set_member(message, NULLORLATIN1(methodName)); }
    void setAutoStart(bool enable)
    { q_dbus_message_set_auto_start(message, enable); }
    void setReplySerial(int serial)
    { q_dbus_message_set_reply_serial(message, serial); }

    QString currentSignature();

    void append(uchar arg);
    void append(bool arg);
    void append(short arg);
    void append(ushort arg);
    void append(int arg);
    void append(uint arg);
    void append(qlonglong arg);
    void append(qulonglong arg);
    void append(double arg);
    void append(const QString &arg);
    void append(const QDBusObjectPath &arg);
    void append(const QDBusSignature &arg);
    void append(const QDBusUnixFileDescriptor &arg);
    void append(const QStringList &arg);
    void append(const QByteArray &arg);
    bool append(const QDBusVariant &arg); // this one can fail

    QDBusMarshaller *beginStructure();
    QDBusMarshaller *endStructure();
    QDBusMarshaller *beginArray(int id);
    QDBusMarshaller *endArray();
    QDBusMarshaller *beginMap(int kid, int vid);
    QDBusMarshaller *endMap();
    QDBusMarshaller *beginMapEntry();
    QDBusMarshaller *endMapEntry();
    QDBusMarshaller *beginCommon(int code, const char *signature);
    QDBusMarshaller *endCommon();
    void error(const QString &message);

    bool appendVariantInternal(const QVariant &arg);
    bool appendRegisteredType(const QVariant &arg);
    bool appendCrossMarshalling(QDBusDemarshaller *arg);

    DBusMessage *stealMessage();

public:
    DBusMessageIter iterator;
    QDBusMarshaller *parent;
    QByteArray *ba;
    QString errorString;
    char closeCode;
    bool ok;
    bool skipSignature;

private:
    QDBusMarshaller(QDBusMarshaller &parent, int subCode, const char *subSignature);

    Q_DISABLE_COPY(QDBusMarshaller)
};

#undef NULLORLATIN1

class QDBusDemarshaller: public QDBusArgumentPrivate
{
public:
    QDBusDemarshaller(DBusMessage *msg, int flags, QDBusDemarshaller *parent = 0);
    ~QDBusDemarshaller();

    QDBusDemarshaller *detachedForReading();

    QDBusMessage::MessageType type()
    { return QDBusMessage::MessageType(q_dbus_message_get_type(message)); }
    QString senderService()
    { return QString::fromUtf8(q_dbus_message_get_sender(message)); }
    QString path()
    { return QString::fromUtf8(q_dbus_message_get_path(message)); }
    QString interface()
    { return QString::fromUtf8(q_dbus_message_get_interface(message)); }
    QString errorName()
    { return QString::fromUtf8(q_dbus_message_get_error_name(message)); }
    QString methodName()
    { return QString::fromUtf8(q_dbus_message_get_member(message)); }
    QString fullSignature()
    { return QString::fromUtf8(q_dbus_message_get_signature(message)); }
    QString currentSignature();

    uchar toByte();
    bool toBool();
    ushort toUShort();
    short toShort();
    int toInt();
    uint toUInt();
    qlonglong toLongLong();
    qulonglong toULongLong();
    double toDouble();
    QString toString();
    QDBusObjectPath toObjectPath();
    QDBusSignature toSignature();
    QDBusUnixFileDescriptor toUnixFileDescriptor();
    QDBusVariant toVariant();
    QStringList toStringList();
    QByteArray toByteArray();

    QDBusDemarshaller *beginStructure();
    QDBusDemarshaller *endStructure();
    QDBusDemarshaller *beginArray();
    QDBusDemarshaller *endArray();
    QDBusDemarshaller *beginMap();
    QDBusDemarshaller *endMap();
    QDBusDemarshaller *beginMapEntry();
    QDBusDemarshaller *endMapEntry();
    QDBusDemarshaller *beginCommon();
    QDBusDemarshaller *endCommon();
    QDBusArgument duplicate();
    inline void close() { }

    bool atEnd();

    QVariant toVariantInternal();
    QDBusArgument::ElementType currentType();
    bool isCurrentTypeStringLike();

    QVariantList demarshal();

public:
    DBusMessageIter iterator;
    QDBusDemarshaller *parent;

private:
    Q_DISABLE_COPY(QDBusDemarshaller)
    QString toStringUnchecked();
    QDBusObjectPath toObjectPathUnchecked();
    QDBusSignature toSignatureUnchecked();
    QStringList toStringListUnchecked();
    QByteArray toByteArrayUnchecked();
};

inline QDBusMarshaller *QDBusArgumentPrivate::marshaller()
{ return static_cast<QDBusMarshaller *>(this); }

inline QDBusDemarshaller *QDBusArgumentPrivate::demarshaller()
{ return static_cast<QDBusDemarshaller *>(this); }

QT_END_NAMESPACE

#endif // QT_NO_DBUS
#endif
