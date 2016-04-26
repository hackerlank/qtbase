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

#include "qmutex.h"
#include <qatomic.h>
#include "qelapsedtimer.h"
#include "qmutex_p.h"

#include <qt_windows.h>

typedef BOOL (WINAPI *ptr_WaitOnAddress_t)(volatile VOID *Address, PVOID CompareAddress, SIZE_T AddressSize, DWORD dwMilliseconds);
typedef VOID (WINAPI *ptr_WakeByAddressSingle_t)(PVOID Address);

QT_BEGIN_NAMESPACE

QMutexPrivate::QMutexPrivate()
{
#ifndef Q_OS_WINRT
    event = CreateEvent(0, FALSE, FALSE, 0);
#else
    event = CreateEventEx(0, NULL, 0, EVENT_ALL_ACCESS);
#endif

    if (!event)
        qWarning("QMutexData::QMutexData: Cannot create event");
}

QMutexPrivate::~QMutexPrivate()
{ CloseHandle(event); }

bool QMutexPrivate::wait(int timeout)
{
    return (WaitForSingleObjectEx(event, timeout < 0 ? INFINITE : timeout, FALSE) == WAIT_OBJECT_0);
}

void QMutexPrivate::wakeUp() Q_DECL_NOTHROW
{ SetEvent(event); }

// The futex-like functions are available since Windows 8
// which happens to be the minimum version for WinRT API anyway
#ifdef Q_OS_WINRT
#  define ptr_WaitOnAddress WaitOnAddress
#  define ptr_WakeByAddressSingle WakeByAddressSingle
static bool futexAvailable()
{
    return true;
}
#else // !Q_OS_WINRT
static ptr_WaitOnAddress_t ptr_WaitOnAddress;
static ptr_WakeByAddressSingle_t ptr_WakeByAddressSingle;
static bool detectFunctions()
{
    // don't use QSystemLibrary here -- we're too low-level and could recurse
    HMODULE kernelbase;
    kernelbase = GetModuleHandleW(L"kernelbase");
    if (kernelbase) {
        ptr_WaitOnAddress = (ptr_WaitOnAddress_t)GetProcAddress(kernelbase, "WaitOnAddress");
        ptr_WakeByAddressSingle = (ptr_WakeByAddressSingle_t)GetProcAddress(kernelbase, "WakeByAddressSingle");
    }
    return true;
}

static bool futexAvailable()
{
    static bool x = detectFunctions();
    Q_UNUSED(x);
    return ptr_WaitOnAddress != nullptr;
}
#endif // !Q_OS_WINRT

static inline QMutexData *dummyFutexValue()
{
    return reinterpret_cast<QMutexData *>(quintptr(3));
}

template <bool IsTimed> static inline
bool lockFutex(QBasicAtomicPointer<QMutexData> &d_ptr, int timeout, QElapsedTimer *elapsedTimer) Q_DECL_NOTHROW
{
    if (!IsTimed)
        timeout = -1;

    // we're here because fastTryLock() has just failed
    if (timeout == 0)
        return false;

    DWORD remainingTime = DWORD(timeout);
    auto waitingValue = dummyFutexValue();

    // the mutex is locked already, set a bit indicating we're waiting
    while (d_ptr.fetchAndStoreAcquire(waitingValue) != nullptr) {
        // it wasn't unlocked, so we need to wait
        bool r = ptr_WaitOnAddress(&d_ptr, &waitingValue, sizeof(waitingValue), remainingTime);
        if (IsTimed && !r) {
            auto err = GetLastError();
            if (err == ERROR_TIMEOUT)
                return false;

            int elapsed = elapsedTimer->elapsed();
            if (timeout > 0) {
                remainingTime = timeout - elapsed;
                if (int(remainingTime) < 0)
                    return false;
            }
        }

        // we got woken up, so try to acquire the mutex
        // note we must set to dummyFutexValue because there could be other threads
        // also waiting
        Q_UNUSED(r);
    }

    Q_ASSERT(d_ptr.load());
    return true;
}

static void unlockFutex(QBasicAtomicPointer<QMutexData> &d_ptr) Q_DECL_NOTHROW
{
    d_ptr.storeRelease(nullptr);
    ptr_WakeByAddressSingle(&d_ptr);
}

QT_END_NAMESPACE
