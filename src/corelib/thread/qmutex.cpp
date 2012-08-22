/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2012 Intel Corporation
** Copyright (C) 2012 Olivier Goffart <ogoffart@woboq.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qplatformdefs.h"
#include "qmutex.h"
#include <qdebug.h>

#ifndef QT_NO_THREAD
#include "qatomic.h"
#include "qelapsedtimer.h"
#include "qthread.h"
#include "qmutex_p.h"
#include "qtypetraits.h"

#ifndef QT_LINUX_FUTEX
#include "private/qfreelist_p.h"
#endif

QT_BEGIN_NAMESPACE

static inline bool isRecursive(QMutexData *d)
{
    quintptr u = quintptr(d);
    if (Q_LIKELY(u <= 0x3))
        return false;
#ifdef QT_LINUX_FUTEX
    Q_ASSERT(d->recursive);
    return true;
#else
    return reinterpret_cast<QMutexData *>(u & ~1)->recursive;
#endif
}

class QRecursiveMutexPrivate : public QMutexData
{
public:
    QRecursiveMutexPrivate()
        : QMutexData(QMutex::Recursive), owner(0), count(0) {}

    // written to by the thread that first owns 'mutex';
    // read during attempts to acquire ownership of 'mutex' from any other thread:
    QAtomicPointer<QtPrivate::remove_pointer<Qt::HANDLE>::type> owner;

    // only ever accessed from the thread that owns 'mutex':
    uint count;

    QMutex mutex;

    bool lock(int timeout) QT_MUTEX_LOCK_NOEXCEPT;
    void unlock() Q_DECL_NOTHROW;
};

/*
    \class QBasicMutex
    \inmodule QtCore
    \brief QMutex POD
    \internal

    \ingroup thread

    - Can be used as global static object.
    - Always non-recursive
    - Do not use tryLock with timeout > 0, else you can have a leak (see the ~QMutex destructor)
*/

/*!
    \class QMutex
    \inmodule QtCore
    \brief The QMutex class provides access serialization between threads.

    \threadsafe

    \ingroup thread

    The purpose of a QMutex is to protect an object, data structure or
    section of code so that only one thread can access it at a time
    (this is similar to the Java \c synchronized keyword). It is
    usually best to use a mutex with a QMutexLocker since this makes
    it easy to ensure that locking and unlocking are performed
    consistently.

    For example, say there is a method that prints a message to the
    user on two lines:

    \snippet code/src_corelib_thread_qmutex.cpp 0

    If these two methods are called in succession, the following happens:

    \snippet code/src_corelib_thread_qmutex.cpp 1

    If these two methods are called simultaneously from two threads then the
    following sequence could result:

    \snippet code/src_corelib_thread_qmutex.cpp 2

    If we add a mutex, we should get the result we want:

    \snippet code/src_corelib_thread_qmutex.cpp 3

    Then only one thread can modify \c number at any given time and
    the result is correct. This is a trivial example, of course, but
    applies to any other case where things need to happen in a
    particular sequence.

    When you call lock() in a thread, other threads that try to call
    lock() in the same place will block until the thread that got the
    lock calls unlock(). A non-blocking alternative to lock() is
    tryLock().

    \sa QMutexLocker, QReadWriteLock, QSemaphore, QWaitCondition
*/

/*!
    \enum QMutex::RecursionMode

    \value Recursive  In this mode, a thread can lock the same mutex
                      multiple times and the mutex won't be unlocked
                      until a corresponding number of unlock() calls
                      have been made.

    \value NonRecursive  In this mode, a thread may only lock a mutex
                         once.

    \sa QMutex()
*/

/*!
    Constructs a new mutex. The mutex is created in an unlocked state.

    If \a mode is QMutex::Recursive, a thread can lock the same mutex
    multiple times and the mutex won't be unlocked until a
    corresponding number of unlock() calls have been made. Otherwise
    a thread may only lock a mutex once. The default is
    QMutex::NonRecursive.

    \sa lock(), unlock()
*/
QMutex::QMutex(RecursionMode mode)
{
    d_ptr.store(mode == Recursive ? new QRecursiveMutexPrivate : 0);
}

/*!
    Destroys the mutex.

    \warning Destroying a locked mutex may result in undefined behavior.
*/
QMutex::~QMutex()
{
    QMutexData *d = d_ptr.load();
    if (isRecursive()) {
        delete static_cast<QRecursiveMutexPrivate *>(d);
    } else if (d) {
        qWarning("QMutex: destroying locked mutex");
    }
}

/*! \fn void QMutex::lock()
    Locks the mutex. If another thread has locked the mutex then this
    call will block until that thread has unlocked it.

    Calling this function multiple times on the same mutex from the
    same thread is allowed if this mutex is a
    \l{QMutex::Recursive}{recursive mutex}. If this mutex is a
    \l{QMutex::NonRecursive}{non-recursive mutex}, this function will
    \e dead-lock when the mutex is locked recursively.

    \sa unlock()
*/
void QMutex::lock() QT_MUTEX_LOCK_NOEXCEPT
{
    QMutexData *current;
    if (fastTryLock(current))
        return;
    if (QT_PREPEND_NAMESPACE(isRecursive)(current))
        static_cast<QRecursiveMutexPrivate *>(current)->lock(-1);
    else
        lockInternal();
}

/*! \fn bool QMutex::tryLock(int timeout)

    Attempts to lock the mutex. This function returns \c true if the lock
    was obtained; otherwise it returns \c false. If another thread has
    locked the mutex, this function will wait for at most \a timeout
    milliseconds for the mutex to become available.

    Note: Passing a negative number as the \a timeout is equivalent to
    calling lock(), i.e. this function will wait forever until mutex
    can be locked if \a timeout is negative.

    If the lock was obtained, the mutex must be unlocked with unlock()
    before another thread can successfully lock it.

    Calling this function multiple times on the same mutex from the
    same thread is allowed if this mutex is a
    \l{QMutex::Recursive}{recursive mutex}. If this mutex is a
    \l{QMutex::NonRecursive}{non-recursive mutex}, this function will
    \e always return false when attempting to lock the mutex
    recursively.

    \sa lock(), unlock()
*/
bool QMutex::tryLock(int timeout) QT_MUTEX_LOCK_NOEXCEPT
{
    QMutexData *current;
    if (fastTryLock(current))
        return true;
    if (QT_PREPEND_NAMESPACE(isRecursive)(current))
        return static_cast<QRecursiveMutexPrivate *>(current)->lock(timeout);
    else
        return lockInternal(timeout);
}

/*! \fn void QMutex::unlock()
    Unlocks the mutex. Attempting to unlock a mutex in a different
    thread to the one that locked it results in an error. Unlocking a
    mutex that is not locked results in undefined behavior.

    \sa lock()
*/
void QMutex::unlock() Q_DECL_NOTHROW
{
    QMutexData *current;
    if (fastTryUnlock(current))
        return;
    if (QT_PREPEND_NAMESPACE(isRecursive)(current))
        static_cast<QRecursiveMutexPrivate *>(current)->unlock();
    else
        unlockInternal();
}

/*!
    \fn void QMutex::isRecursive()
    \since 5.0

    Returns \c true if the mutex is recursive

*/
bool QBasicMutex::isRecursive()
{
    return QT_PREPEND_NAMESPACE(isRecursive)(d_ptr.loadAcquire());
}


/*!
    \class QMutexLocker
    \inmodule QtCore
    \brief The QMutexLocker class is a convenience class that simplifies
    locking and unlocking mutexes.

    \threadsafe

    \ingroup thread

    Locking and unlocking a QMutex in complex functions and
    statements or in exception handling code is error-prone and
    difficult to debug. QMutexLocker can be used in such situations
    to ensure that the state of the mutex is always well-defined.

    QMutexLocker should be created within a function where a
    QMutex needs to be locked. The mutex is locked when QMutexLocker
    is created. You can unlock and relock the mutex with \c unlock()
    and \c relock(). If locked, the mutex will be unlocked when the
    QMutexLocker is destroyed.

    For example, this complex function locks a QMutex upon entering
    the function and unlocks the mutex at all the exit points:

    \snippet code/src_corelib_thread_qmutex.cpp 4

    This example function will get more complicated as it is
    developed, which increases the likelihood that errors will occur.

    Using QMutexLocker greatly simplifies the code, and makes it more
    readable:

    \snippet code/src_corelib_thread_qmutex.cpp 5

    Now, the mutex will always be unlocked when the QMutexLocker
    object is destroyed (when the function returns since \c locker is
    an auto variable).

    The same principle applies to code that throws and catches
    exceptions. An exception that is not caught in the function that
    has locked the mutex has no way of unlocking the mutex before the
    exception is passed up the stack to the calling function.

    QMutexLocker also provides a \c mutex() member function that returns
    the mutex on which the QMutexLocker is operating. This is useful
    for code that needs access to the mutex, such as
    QWaitCondition::wait(). For example:

    \snippet code/src_corelib_thread_qmutex.cpp 6

    \sa QReadLocker, QWriteLocker, QMutex
*/

/*!
    \fn QMutexLocker::QMutexLocker(QMutex *mutex)

    Constructs a QMutexLocker and locks \a mutex. The mutex will be
    unlocked when the QMutexLocker is destroyed. If \a mutex is zero,
    QMutexLocker does nothing.

    \sa QMutex::lock()
*/

/*!
    \fn QMutexLocker::~QMutexLocker()

    Destroys the QMutexLocker and unlocks the mutex that was locked
    in the constructor.

    \sa QMutex::unlock()
*/

/*!
    \fn void QMutexLocker::unlock()

    Unlocks this mutex locker. You can use \c relock() to lock
    it again. It does not need to be locked when destroyed.

    \sa relock()
*/

/*!
    \fn void QMutexLocker::relock()

    Relocks an unlocked mutex locker.

    \sa unlock()
*/

/*!
    \fn QMutex *QMutexLocker::mutex() const

    Returns the mutex on which the QMutexLocker is operating.

*/

#ifndef QT_LINUX_FUTEX //linux implementation is in qmutex_linux.cpp
static inline QMutexData *dummyLocked()
{
    return reinterpret_cast<QMutexData *>(quintptr(1));
}

static inline bool isSpinLocked(QMutexData *d)
{
    Q_ASSERT(d != dummyLocked());
    return quintptr(d) & 0x1;
}

static inline QMutexData *spinLocked(QMutexData *d)
{
    return reinterpret_cast<QMutexData *>(quintptr(d) | quintptr(1));
}

static inline QMutexData *spinUnlocked(QMutexData *d)
{
    return reinterpret_cast<QMutexData *>(quintptr(d) & quintptr(~1));
}

// this function must be called by the thread that holds the spinlock
static inline void derefAndUnlock(QBasicAtomicPointer<QMutexData> &d_ptr, QMutexPrivate *expected)
{
    // spin-lock the pointer before updating the reference count
    while (true) {
        if (d_ptr.testAndSetAcquire(expected, spinLocked(expected)))
            break;
        loopPause();
        Q_ASSERT(d_ptr.load() != 0);
        Q_ASSERT(d_ptr.load() != dummyLocked());
    }

    if (--expected->refCount) {
        // not the last reference count, simply unlock
        d_ptr.storeRelease(expected);
    } else {
        // last reference being dropped, release completely
        d_ptr.storeRelease(0);
        expected->release();
    }
}

template <bool IsTimed> static inline
bool lockInternal_helper(QBasicAtomicPointer<QMutexData> &d_ptr, int timeout = -1) QT_MUTEX_LOCK_NOEXCEPT
{
    Q_ASSERT(spinLocked(0) == dummyLocked());
    if (!IsTimed)
        timeout = -1;

    if (timeout == 0)
        return false;

    unsigned int retries = AdaptiveLockRetries;
    while (true) {
        loopPause();
        if (d_ptr.testAndSetAcquire(0, dummyLocked()))
            return true;
        if (!--retries)
            break;
    }

    QElapsedTimer elapsedTimer;
    if (IsTimed && timeout > 0)
        elapsedTimer.start();

    // acquire a private and ref it up
    QMutexPrivate *d = 0;
    while (!d_ptr.testAndSetAcquire(0, dummyLocked())) {
        QMutexData *copy = d_ptr.load();
        loopPause();
        copy = spinUnlocked(copy);

        if (d_ptr.testAndSetAcquire(copy, spinLocked(copy))) {
            if (!copy) {
                // this QMutex was unlocked, we've locked it
                return true;
            }

            // this QMutex was contended, we've spin-locked it
            d = static_cast<QMutexPrivate *>(copy);
            ++d->refCount;
            d_ptr.storeRelease(copy);

            break;
        }

        copy = d_ptr.load();
        if (copy == dummyLocked()) {
            // this QMutex is locked, but not contended
            // set the contention structure
            QMutexPrivate *newD = QMutexPrivate::allocate();
            newD->refCount = 2; // this thread and the other one that has it locked

            if (!d_ptr.testAndSetOrdered(copy, newD)) {
                //Either the mutex is already unlocked, or another thread already set it.
                newD->refCount = 0;
                newD->release();
                continue;
            }

            // successfully set the contention structure
            d = newD;
            break;
        }
    }

    // successfully set the waiting struct, now sleep
    if (IsTimed && timeout > 0) {
        int xtimeout = timeout - elapsedTimer.elapsed();
        if (xtimeout < 0 || !d->wait(xtimeout)) {
            // timed lock failed to acquire the lock
            derefAndUnlock(d_ptr, d);
            return false;
        }
    } else {
        // not timed, this can't fail
        d->wait();
    }

    Q_ASSERT(quintptr(d_ptr.load()) > 0x3);
    return true;
}

/*!
    \internal helper for lock()
 */
void QBasicMutex::lockInternal() QT_MUTEX_LOCK_NOEXCEPT
{
    Q_ASSERT(!isRecursive());
    lockInternal_helper<false>(d_ptr);
}

/*!
    \internal helper for lock(int)
 */
bool QBasicMutex::lockInternal(int timeout) QT_MUTEX_LOCK_NOEXCEPT
{
    Q_ASSERT(!isRecursive());
    return lockInternal_helper<true>(d_ptr, timeout);
}

/*!
    \internal
*/
void QBasicMutex::unlockInternal() Q_DECL_NOTHROW
{
    // loadAcquire because refCount is changed in other threads
    QMutexData *copy = spinUnlocked(d_ptr.loadAcquire());
    Q_ASSERT(copy); //we must be locked
    Q_ASSERT(copy != dummyLocked()); // testAndSetRelease(dummyLocked(), 0) failed
    Q_ASSERT(!isRecursive());


    // we hold a reference, so d will not disappear under us
    QMutexPrivate *d = reinterpret_cast<QMutexPrivate *>(copy);
    d->wakeUp();

    derefAndUnlock(d_ptr, d);
}

//The freelist management
namespace {
struct FreeListConstants : QFreeListDefaultConstants {
    enum { BlockCount = 4, MaxIndex=0xffff };
    static const int Sizes[BlockCount];
};
const int FreeListConstants::Sizes[FreeListConstants::BlockCount] = {
    16,
    128,
    1024,
    FreeListConstants::MaxIndex - (16-128-1024)
};

typedef QFreeList<QMutexPrivate, FreeListConstants> FreeList;
Q_GLOBAL_STATIC(FreeList, freelist);
}

QMutexPrivate *QMutexPrivate::allocate()
{
    int i = freelist()->next();
    QMutexPrivate *d = &(*freelist())[i];
    d->id = i;
    Q_ASSERT(d->refCount == 0);
    Q_ASSERT(!d->recursive);
#if defined Q_OS_UNIX && !defined Q_OS_MAC
    Q_ASSERT(!d->wakeup);
#endif
    return d;
}

void QMutexPrivate::release()
{
    Q_ASSERT(!recursive);
    Q_ASSERT(refCount == 0);
    eatSignalling();
    freelist()->release(id);
}
#endif

/*!
   \internal
 */
inline bool QRecursiveMutexPrivate::lock(int timeout) QT_MUTEX_LOCK_NOEXCEPT
{
    Qt::HANDLE self = QThread::currentThreadId();
    if (owner.load() == self) {
        ++count;
        Q_ASSERT_X(count != 0, "QMutex::lock", "Overflow in recursion counter");
        return true;
    }
    bool success = true;
    if (timeout == -1) {
        mutex.QBasicMutex::lock();
    } else {
        success = mutex.tryLock(timeout);
    }

    if (success)
        owner.store(self);
    return success;
}

/*!
   \internal
 */
inline void QRecursiveMutexPrivate::unlock() Q_DECL_NOTHROW
{
    if (count > 0) {
        count--;
    } else {
        owner.store(0);
        mutex.QBasicMutex::unlock();
    }
}

QT_END_NAMESPACE

#ifdef QT_LINUX_FUTEX
#  include "qmutex_linux.cpp"
#elif defined(Q_OS_MAC)
#  include "qmutex_mac.cpp"
#elif defined(Q_OS_WIN)
#  include "qmutex_win.cpp"
#else
#  include "qmutex_unix.cpp"
#endif

#endif // QT_NO_THREAD
