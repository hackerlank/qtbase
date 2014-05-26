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

#ifndef QARRAYDATAPOINTER_H
#define QARRAYDATAPOINTER_H

#include <QtCore/qarraydataops.h>

#if __cplusplus >= 201103L
#  include <type_traits>
#endif

QT_BEGIN_NAMESPACE

template <class T>
struct QBasicArrayDataPointer
{
protected:
    typedef QTypedArrayData<T> Data;
    typedef QArrayDataOps<T> DataOps;

public:
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;
#if __cplusplus >= 201103L
    typedef typename std::conditional<std::is_fundamental<T>::value || std::is_pointer<T>::value, T, const T &>::type parameter_type;
#else
    typedef const T &parameter_type;
#endif

    QBasicArrayDataPointer() = default; // you probably don't want this
    constexpr QBasicArrayDataPointer(Data *header, T *adata, size_t n = 0) Q_DECL_NOTHROW
        : d(header), ptr(adata), size(uint(n))
    {
    }

    DataOps &operator*() Q_DECL_NOTHROW
    {
        Q_ASSERT(d);
        return *static_cast<DataOps *>(this);
    }

    DataOps *operator->() Q_DECL_NOTHROW
    {
        Q_ASSERT(d);
        return static_cast<DataOps *>(this);
    }

    const DataOps &operator*() const Q_DECL_NOTHROW
    {
        Q_ASSERT(d);
        return *static_cast<const DataOps *>(this);
    }

    const DataOps *operator->() const Q_DECL_NOTHROW
    {
        Q_ASSERT(d);
        return static_cast<const DataOps *>(this);
    }

    bool isNull() const Q_DECL_NOTHROW
    {
        return d == Data::sharedNull();
    }

    T *data() Q_DECL_NOTHROW { return ptr; }
    const T *data() const Q_DECL_NOTHROW { return ptr; }

    iterator begin(iterator = iterator()) Q_DECL_NOTHROW { return data(); }
    iterator end(iterator = iterator()) Q_DECL_NOTHROW { return data() + size; }
    const_iterator begin(const_iterator = const_iterator()) const Q_DECL_NOTHROW { return data(); }
    const_iterator end(const_iterator = const_iterator()) const Q_DECL_NOTHROW { return data() + size; }
    const_iterator constBegin(const_iterator = const_iterator()) const Q_DECL_NOTHROW { return data(); }
    const_iterator constEnd(const_iterator = const_iterator()) const Q_DECL_NOTHROW { return data() + size; }

#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
    bool isSharable() const { return d->isSharable(); }
#endif

    void swap(QBasicArrayDataPointer &other) Q_DECL_NOTHROW
    {
        qSwap(d, other.d);
        qSwap(ptr, other.ptr);
        qSwap(size, other.size);
    }

    inline void clear() Q_DECL_NOEXCEPT_EXPR(std::is_nothrow_destructible<T>::value);
    inline bool detach();

    // forwards from QArrayData
    size_t allocatedCapacity() Q_DECL_NOTHROW { return d->allocatedCapacity(); }
    size_t constAllocatedCapacity() const Q_DECL_NOTHROW { return d->constAllocatedCapacity(); }
    int refCounterValue() const Q_DECL_NOTHROW { return d->refCounterValue(); }
    bool ref() Q_DECL_NOTHROW { return d->ref(); }
    bool deref() Q_DECL_NOTHROW { return d->deref(); }
    bool isMutable() const Q_DECL_NOTHROW { return d->isMutable(); }
    bool isStatic() const Q_DECL_NOTHROW { return d->isStatic(); }
    bool isShared() const Q_DECL_NOTHROW { return d->isShared(); }
    bool needsDetach() const Q_DECL_NOTHROW { return d->needsDetach(); }
    size_t detachCapacity(size_t newSize) const Q_DECL_NOTHROW { return d->detachCapacity(newSize); }
    typename Data::ArrayOptions &flags() Q_DECL_NOTHROW { return reinterpret_cast<typename Data::ArrayOptions &>(d->flags); }
    typename Data::ArrayOptions flags() const Q_DECL_NOTHROW { return typename Data::ArrayOption(d->flags); }
    typename Data::ArrayOptions detachFlags() const Q_DECL_NOTHROW { return d->detachFlags(); }
    typename Data::ArrayOptions cloneFlags() const Q_DECL_NOTHROW { return d->cloneFlags(); }

protected:
    union {
        Data *d;
        QArrayAllocatedData *ad;
    };
    T *ptr;

public:
    uint size;
};

template <class T>
struct QArrayDataPointer : public QBasicArrayDataPointer<T>
{
    typedef typename QBasicArrayDataPointer<T>::Data Data;

    QArrayDataPointer() Q_DECL_NOTHROW
        : QBasicArrayDataPointer<T>(Data::sharedNull(), Data::sharedNullData())
    {
    }

    constexpr QArrayDataPointer(Data *header, T *adata, size_t n = 0) Q_DECL_NOTHROW
        : QBasicArrayDataPointer<T>(header, adata, n)
    {
    }

    QArrayDataPointer(const QArrayDataPointer &other)
        : QBasicArrayDataPointer<T>(other)
#ifndef QT_NO_UNSHARABLE_CONTAINERS
    Q_DECL_NOTHROW
#endif
    {
        if (!other.d->ref()) {
            // must clone
            QPair<Data *, T *> pair = other.clone(other->cloneFlags());
            this->d = pair.first;
            this->ptr = pair.second;
        }
    }

    QArrayDataPointer(QArrayDataPointer &&other) Q_DECL_NOTHROW
        : QBasicArrayDataPointer<T>(std::move(other))
    {
        other.d = Data::sharedNull();
        other.ptr = Data::sharedNullData();
        other.size = 0;
    }

    explicit QArrayDataPointer(QPair<QTypedArrayData<T> *, T *> adata, size_t n = 0)
        : QBasicArrayDataPointer<T>(adata.first, adata.second, n)
    {
        Q_CHECK_PTR(adata.first);
    }

    QArrayDataPointer(QArrayDataPointerRef<T> dd) Q_DECL_NOTHROW
        : QBasicArrayDataPointer<T>(dd.ptr, dd.data, dd.size)
    {
    }

    QArrayDataPointer &operator=(const QArrayDataPointer &other)
#ifdef QT_NO_UNSHARABLE_CONTAINERS
    Q_DECL_NOTHROW
#endif
    {
        QArrayDataPointer tmp(other);
        this->swap(tmp);
        return *this;
    }

    QArrayDataPointer &operator=(QArrayDataPointer &&other) Q_DECL_NOTHROW
    {
        QArrayDataPointer moved(std::move(other));
        this->swap(moved);
        return *this;
    }

    ~QArrayDataPointer()
    {
        if (!this->deref()) {
            if (this->isMutable())
                (*this)->destroyAll();
            Data::deallocate(this->d);
        }
    }

    void clear() Q_DECL_NOEXCEPT_EXPR(std::is_nothrow_destructible<T>::value)
    {
        QArrayDataPointer tmp;
        swap(tmp);
    }

    bool detach()
    {
        if (this->needsDetach()) {
            QPair<Data *, T *> copy = clone(this->detachFlags());
            QArrayDataPointer old(this->d, this->ptr, this->size);
            this->d = copy.first;
            this->ptr = copy.second;
            return true;
        }

        return false;
    }

#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
    void setSharable(bool sharable)
    {
        if (this->needsDetach()) {
            QPair<Data *, T *> detached;
            detached = clone(sharable
                    ? this->detachFlags() & ~QArrayData::Unsharable
                    : this->detachFlags() | QArrayData::Unsharable);
            QArrayDataPointer old(this->d, this->ptr, this->size);
            this->d = detached.first;
            this->ptr = detached.second;
        } else {
            this->d->setSharable(sharable);
        }
    }
#endif

private:
    QPair<Data *, T *> clone(QArrayData::ArrayOptions options) const Q_REQUIRED_RESULT
    {
        QPair<Data *, T *> pair = Data::allocate(this->detachCapacity(this->size),
                    options);
        Q_CHECK_PTR(pair.first);
        QArrayDataPointer copy(pair.first, pair.second, 0);
        if (this->size)
            copy->copyAppend(this->begin(), this->end());

        pair.first = copy.d;
        copy.d = Data::sharedNull();
        return pair;
    }
};

template <class T> inline void
QBasicArrayDataPointer<T>::clear() Q_DECL_NOEXCEPT_EXPR(std::is_nothrow_destructible<T>::value)
{
    static_cast<QArrayDataPointer<T> *>(this)->clear();
}

template <class T> inline bool
QBasicArrayDataPointer<T>::detach()
{
    return static_cast<QArrayDataPointer<T> *>(this)->detach();
}

template <class T>
inline bool operator==(const QArrayDataPointer<T> &lhs, const QArrayDataPointer<T> &rhs) Q_DECL_NOTHROW
{
    return lhs.data() == rhs.data() && lhs.size == rhs.size;
}

template <class T>
inline bool operator!=(const QArrayDataPointer<T> &lhs, const QArrayDataPointer<T> &rhs) Q_DECL_NOTHROW
{
    return lhs.data() != rhs.data() || lhs.size != rhs.size;
}

template <class T>
inline void qSwap(QArrayDataPointer<T> &p1, QArrayDataPointer<T> &p2) Q_DECL_NOTHROW
{
    p1.swap(p2);
}

QT_END_NAMESPACE

namespace std
{
    template <class T>
    inline void swap(
            QT_PREPEND_NAMESPACE(QArrayDataPointer)<T> &p1,
            QT_PREPEND_NAMESPACE(QArrayDataPointer)<T> &p2) Q_DECL_NOTHROW
    {
        p1.swap(p2);
    }
}

#endif // include guard
