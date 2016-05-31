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

#ifndef QDBUSCONNECTION_H
#define QDBUSCONNECTION_H

#include <QtDBus/qtdbusglobal.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>

#ifndef QT_NO_DBUS

QT_BEGIN_NAMESPACE

namespace QDBus
{
    enum CallMode {
        NoBlock,
        Block,
        BlockWithGui,
        AutoDetect
    };
}

class QDBusAbstractInterfacePrivate;
class QDBusInterface;
class QDBusError;
class QDBusMessage;
class QDBusPendingCall;
class QDBusConnectionInterface;
class QDBusVirtualObject;
class QObject;

class QDBusConnectionPrivate;
class Q_DBUS_EXPORT QDBusConnection
{
    Q_GADGET
    Q_ENUMS(BusType UnregisterMode)
    Q_FLAGS(RegisterOptions)
public:
    enum BusType { SessionBus, SystemBus, ActivationBus };
    enum RegisterOption {
        ExportAdaptors = 0x01,

        ExportScriptableSlots = 0x10,
        ExportScriptableSignals = 0x20,
        ExportScriptableProperties = 0x40,
        ExportScriptableInvokables = 0x80,
        ExportScriptableContents = 0xf0,

        ExportNonScriptableSlots = 0x100,
        ExportNonScriptableSignals = 0x200,
        ExportNonScriptableProperties = 0x400,
        ExportNonScriptableInvokables = 0x800,
        ExportNonScriptableContents = 0xf00,

        ExportAllSlots = ExportScriptableSlots|ExportNonScriptableSlots,
        ExportAllSignals = ExportScriptableSignals|ExportNonScriptableSignals,
        ExportAllProperties = ExportScriptableProperties|ExportNonScriptableProperties,
        ExportAllInvokables = ExportScriptableInvokables|ExportNonScriptableInvokables,
        ExportAllContents = ExportScriptableContents|ExportNonScriptableContents,

#ifndef Q_QDOC
        // Qt 4.2 had a misspelling here
        ExportAllSignal = ExportAllSignals,
#endif
        ExportChildObjects = 0x1000
        // Reserved = 0xff000000
    };
    Q_DECLARE_FLAGS(RegisterOptions, RegisterOption)

    enum UnregisterMode {
        UnregisterNode,
        UnregisterTree
    };

    enum VirtualObjectRegisterOption {
        SingleNode = 0x0,
        SubPath = 0x1
        // Reserved = 0xff000000
    };
#ifndef Q_QDOC
    Q_DECLARE_FLAGS(VirtualObjectRegisterOptions, VirtualObjectRegisterOption)
#endif

    enum ConnectionCapability {
        UnixFileDescriptorPassing = 0x0001
    };
    Q_DECLARE_FLAGS(ConnectionCapabilities, ConnectionCapability)

    class Connection {
        void *d_ptr;
        explicit Connection(void *data) : d_ptr(data) {  }
        friend class QDBusConnectionPrivate;
    public:
        ~Connection();
        Connection() : d_ptr(nullptr) {}
        Connection(const Connection &other);
        Connection &operator=(const Connection &other);
#ifdef Q_QDOC
        operator bool() const;
#else
        typedef void *Connection::*RestrictedBool;
        operator RestrictedBool() const { return d_ptr ? &Connection::d_ptr : nullptr; }
#endif

        inline Connection(Connection &&o) : d_ptr(o.d_ptr) { o.d_ptr = nullptr; }
        inline Connection &operator=(Connection &&other)
        { qSwap(d_ptr, other.d_ptr); return *this; }

        void swap(QDBusConnection::Connection &other) Q_DECL_NOTHROW
        { qSwap(d_ptr, other.d_ptr); }
    };

    explicit QDBusConnection(const QString &name);
    QDBusConnection(const QDBusConnection &other);
#ifdef Q_COMPILER_RVALUE_REFS
    QDBusConnection(QDBusConnection &&other) Q_DECL_NOTHROW : d(other.d) { other.d = Q_NULLPTR; }
    QDBusConnection &operator=(QDBusConnection &&other) Q_DECL_NOTHROW { swap(other); return *this; }
#endif
    QDBusConnection &operator=(const QDBusConnection &other);
    ~QDBusConnection();

    void swap(QDBusConnection &other) Q_DECL_NOTHROW { qSwap(d, other.d); }

    bool isConnected() const;
    QString baseService() const;
    QDBusError lastError() const;
    QString name() const;
    ConnectionCapabilities connectionCapabilities() const;

    bool send(const QDBusMessage &message) const;
    bool callWithCallback(const QDBusMessage &message, QObject *receiver,
                          const char *returnMethod, const char *errorMethod,
                          int timeout = -1) const;
    bool callWithCallback(const QDBusMessage &message, QObject *receiver,
                          const char *slot, int timeout = -1) const;
    QDBusMessage call(const QDBusMessage &message, QDBus::CallMode mode = QDBus::Block,
                      int timeout = -1) const;
    QDBusPendingCall asyncCall(const QDBusMessage &message, int timeout = -1) const;

    bool connect(const QString &service, const QString &path, const QString &interface,
                 const QString &name, QObject *receiver, const char *slot);
    bool connect(const QString &service, const QString &path, const QString &interface,
                 const QString &name, const QString& signature,
                 QObject *receiver, const char *slot);
    bool connect(const QString &service, const QString &path, const QString &interface,
                 const QString &name, const QStringList &argumentMatch, const QString& signature,
                 QObject *receiver, const char *slot);

    bool disconnect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, const QString& signature,
                    QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, const QStringList &argumentMatch, const QString& signature,
                    QObject *receiver, const char *slot);
    bool disconnect(const Connection &connection);

    bool registerObject(const QString &path, QObject *object,
                        RegisterOptions options = ExportAdaptors);
    bool registerObject(const QString &path, const QString &interface, QObject *object,
                        RegisterOptions options = ExportAdaptors);
    void unregisterObject(const QString &path, UnregisterMode mode = UnregisterNode);
    QObject *objectRegisteredAt(const QString &path) const;

    bool registerVirtualObject(const QString &path, QDBusVirtualObject *object,
                          VirtualObjectRegisterOption options = SingleNode);

    bool registerService(const QString &serviceName);
    bool unregisterService(const QString &serviceName);

    QDBusConnectionInterface *interface() const;

    void *internalPointer() const;

    static QDBusConnection connectToBus(BusType type, const QString &name);
    static QDBusConnection connectToBus(const QString &address, const QString &name);
    static QDBusConnection connectToPeer(const QString &address, const QString &name);
    static void disconnectFromBus(const QString &name);
    static void disconnectFromPeer(const QString &name);

    static QByteArray localMachineId();

    static QDBusConnection sessionBus();
    static QDBusConnection systemBus();

#if QT_DEPRECATED_SINCE(5,5)
    static QT_DEPRECATED_X("This function no longer works, use QDBusContext instead")
    QDBusConnection sender();
#endif

#ifdef Q_QDOC
    QDBusConnection::Connection connect(const QString &service, const QString &path, const QString &interface,
                                        const QString &name, const QStringList &argumentMatch,
                                        const QObject *receiver, PointerToMemberFunction method);
    QDBusConnection::Connection connect(const QString &service, const QString &path, const QString &interface,
                                        const QString &name, const QStringList &argumentMatch,
                                        const QObject *context, Functor functor);
#elif !defined(QT_NO_QOBJECT)
    //Connect a D-Bus signal to a pointer to qobject member function
    template <typename Func2>
    Connection connect(const QString &service, const QString &path, const QString &interface,
                       const QString &name, const QStringList &argumentMatch,
                       typename QtPrivate::FunctionPointer<Func2>::Object *receiver, Func2 slot)
    {
        using namespace QtPrivate;
        typedef FunctionPointer<Func2> SlotType;
        typedef QSlotObject<Func2, typename SlotType::Arguments, void> SlotObject;
        typedef typename SlotType::template ChangeClass<QObject>::Type StoredFunc2;
        StoredFunc2 storedSlot = static_cast<StoredFunc2>(slot);

        const int *types = ConnectionTypes<typename SlotType::Arguments>::types();
        if (SlotType::ArgumentCount == 0)
            types = reinterpret_cast<const int *>(1);

        return connectImpl(service, path, interface, name, argumentMatch,
                           receiver, reinterpret_cast<void **>(&storedSlot),
                           new SlotObject(slot), types);
    }

    // Connect a D-Bus signal to a static member or non-member function
    template <typename Func2>
    typename QtPrivate::QEnableIf<int(QtPrivate::FunctionPointer<Func2>::ArgumentCount) >= 0 &&
                                  !QtPrivate::FunctionPointer<Func2>::IsPointerToMemberFunction, Connection>::Type
            connect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, const QStringList &argumentMatch,
                    QObject *context, Func2 slot)
    {
        using namespace QtPrivate;
        typedef FunctionPointer<Func2> SlotType;
        typedef QStaticSlotObject<Func2, typename SlotType::Arguments, void> SlotObject;

        const int *types = ConnectionTypes<typename SlotType::Arguments>::types();
        if (SlotType::ArgumentCount == 0)
            types = reinterpret_cast<const int *>(1);

        return connectImpl(service, path, interface, name, argumentMatch,
                           context, nullptr, new SlotObject(slot), types);
    }

    // Connect a D-Bus signal to a functor
    template <typename Func2>
    typename QtPrivate::QEnableIf<int(QtPrivate::FunctionPointer<Func2>::ArgumentCount) == -1, Connection>::Type
            connect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, const QStringList &argumentMatch,
                    QObject *context, Func2 slot)
    {
        using namespace QtPrivate;
        typedef FunctionPointer<decltype(&Func2::operator())> SlotType;
        typedef QFunctorSlotObject<Func2, SlotType::ArgumentCount, typename SlotType::Arguments, void> SlotObject;

        const int *types = ConnectionTypes<typename SlotType::Arguments>::types();
        if (SlotType::ArgumentCount == 0)
            types = reinterpret_cast<const int *>(1);

        return connectImpl(service, path, interface, name, argumentMatch,
                           context, nullptr, new SlotObject(slot), types);
    }

private:
    Connection connectImpl(const QString &service, const QString &path, const QString &interface,
                           const QString &name, const QStringList &argumentMatch,
                           QObject *receiver, void **slot, QtPrivate::QSlotObjectBase *slotObj,
                           const int *types);
#endif // !Q_QDOC && !QT_NO_QOBJECT

protected:
    explicit QDBusConnection(QDBusConnectionPrivate *dd);

private:
    friend class QDBusConnectionPrivate;
    QDBusConnectionPrivate *d;
};
Q_DECLARE_SHARED_NOT_MOVABLE_UNTIL_QT6(QDBusConnection)
Q_DECLARE_SHARED(QDBusConnection::Connection)

Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusConnection::RegisterOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusConnection::VirtualObjectRegisterOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusConnection::ConnectionCapabilities)

QT_END_NAMESPACE

#endif // QT_NO_DBUS
#endif
