/****************************************************************************
**
** Copyright (C) 2013 Intel Corporation
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGENERICARRAY_H
#define QGENERICARRAY_H

#include <QtCore/qarraydatapointer.h>
#include <QtCore/qnamespace.h>
#include <limits>

#ifdef Q_COMPILER_INITIALIZER_LISTS
#include <initializer_list>
#endif

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

template <typename T>
class QGenericArray
{
    typedef QTypedArrayData<T> Data;
    typedef QArrayDataOps<T> DataOps;
    typedef QArrayDataPointer<T> DataPointer;

public:
    typedef T Type;
    typedef T value_type;
    typedef value_type *pointer;
    typedef const value_type *const_pointer;
    typedef value_type &reference;
    typedef const value_type &const_reference;
    typedef int size_type;
    typedef qptrdiff difference_type;
    typedef typename Data::iterator iterator;
    typedef typename Data::const_iterator const_iterator;
    typedef iterator Iterator;
    typedef const_iterator ConstIterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

protected:
    typedef typename DataPointer::parameter_type parameter_type;

    // this class is not meant to be used directly
    // for that reason, constructors and destructors are protected

    QGenericArray() Q_DECL_NOEXCEPT {}
    //~QGenericArray() Q_DECL_NOEXCEPT_EXPR(std::is_nothrow_destructible<T>::value) {}

    QGenericArray(const QGenericArray &other) Q_DECL_NOEXCEPT
        : d(other.d)
    {
    }

    // creates n default-constructed elements
    explicit QGenericArray(size_t n)
        : d(Data::allocate(n))
    {
        if (n)
            d->appendInitialize(n);
    }

    QGenericArray(size_t n, parameter_type t)
        : d(Data::allocate(n))
    {
        if (n)
            d->copyAppend(n, t);
    }

    QGenericArray(size_t n, Qt::Initialization)
        : d(Data::allocate(n))
    {
    }

    QGenericArray(const_iterator i1, const_iterator i2)
        : d(Data::allocate(std::distance(i1, i2)))
    {
        if (std::distance(i1, i2))
            d->copyAppend(i1, i2);
    }

    QGenericArray &operator=(const QGenericArray &other)
    {
        d = other.d;
        return *this;
    }

#ifdef Q_COMPILER_RVALUE_REFS
    QGenericArray(QGenericArray &&other) Q_DECL_NOEXCEPT
        : d(std::move(other.d))
    {
    }

    QGenericArray &operator=(QGenericArray &&other) Q_DECL_NOEXCEPT_EXPR(std::is_nothrow_destructible<T>::value)
    {
        d = std::move(other.d);
        return *this;
    }
#endif

#ifdef Q_COMPILER_INITIALIZER_LISTS
    QGenericArray(std::initializer_list<T> args)
        : d(Data::allocate(args.size()))
    {
        if (args.size())
            d->copyAppend(args.begin(), args.end());
    }

#endif

    // not to be made public:
    QGenericArray(DataPointer dd) Q_DECL_NOEXCEPT
        : d(dd)
    {
    }

    // The following methods take the wrong type, so they're protected
    void swap(QGenericArray &other) Q_DECL_NOEXCEPT { qSwap(d, other.d); }
    bool isSharedWith(const QGenericArray &other) const Q_DECL_NOEXCEPT
    { return d == other.d; }
    void fill(parameter_type t, int size); // should return *this
    DataPointer mid(int pos, int length) const;

public:
    bool isEmpty() const Q_DECL_NOEXCEPT { return d->size == 0; }
    void clear() { resize(0); }
    int size() const Q_DECL_NOEXCEPT { return int(d->size); }
    int count() const Q_DECL_NOEXCEPT { return size(); }
    int length() const Q_DECL_NOEXCEPT { return size(); }

    void resize(int i)
    {
        resize_internal(i, Qt::Uninitialized);
        if (i > size())
            d->appendInitialize(i);
    }

    void resize(int i, parameter_type c)
    {
        resize_internal(i, Qt::Uninitialized);
        if (i > size())
            d->copyAppend(i - size(), c);
    }

    int capacity() const Q_DECL_NOEXCEPT { return int(d->constAllocatedCapacity()); }
    void squeeze();
    void reserve(int i);

    bool isDetached() const Q_DECL_NOEXCEPT { return !d->isShared(); }
    void detach() { d.detach(); }

    friend bool operator==(const QGenericArray &l, const QGenericArray &r)
    {
        if (l.size() != r.size())
            return false;
        if (l.begin() == r.begin())
            return true;

        // do element-by-element comparison
        return l.d->compare(l.begin(), r.begin(), l.size());
    }
    friend bool operator!=(const QGenericArray &l, const QGenericArray &r)
    {
        return !(l == r);
    }

    pointer data() { detach(); return d->data(); }
    const_pointer data() const Q_DECL_NOEXCEPT { return d->data(); }
    const_pointer constData() const Q_DECL_NOEXCEPT { return d->data(); }

#if !defined(QT_STRICT_ITERATORS) || defined(Q_QDOC)
    iterator begin() { detach(); return d->begin(); }
    iterator end() { detach(); return d->end(); }

    const_iterator begin() const Q_DECL_NOEXCEPT { return d->constBegin(); }
    const_iterator end() const Q_DECL_NOEXCEPT { return d->constEnd(); }
    const_iterator cbegin() const Q_DECL_NOEXCEPT { return d->constBegin(); }
    const_iterator cend() const Q_DECL_NOEXCEPT { return d->constEnd(); }
    const_iterator constBegin() const Q_DECL_NOEXCEPT { return d->constBegin(); }
    const_iterator constEnd() const Q_DECL_NOEXCEPT { return d->constEnd(); }
#else
    iterator begin(iterator = iterator()) Q_DECL_NOEXCEPT { detach(); return d->begin(); }
    iterator end(iterator = iterator()) Q_DECL_NOEXCEPT { detach(); return d->end(); }

    const_iterator begin(const_iterator = const_iterator()) const Q_DECL_NOEXCEPT { return d->constBegin(); }
    const_iterator cbegin(const_iterator = const_iterator()) const Q_DECL_NOEXCEPT { return d->constBegin(); }
    const_iterator constBegin(const_iterator = const_iterator()) const Q_DECL_NOEXCEPT { return d->constBegin(); }
    const_iterator end(const_iterator = const_iterator()) const Q_DECL_NOEXCEPT { return d->constEnd(); }
    const_iterator cend(const_iterator = const_iterator()) const Q_DECL_NOEXCEPT { return d->constEnd(); }
    const_iterator constEnd(const_iterator = const_iterator()) const Q_DECL_NOEXCEPT { return d->constEnd(); }
#endif
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const Q_DECL_NOTHROW { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const Q_DECL_NOTHROW { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const Q_DECL_NOTHROW { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const Q_DECL_NOTHROW { return const_reverse_iterator(begin()); }

    reference first()
    {
        Q_ASSERT(!isEmpty());
        detach();
        return data()[0];
    }

    reference last()
    {
        Q_ASSERT(!isEmpty());
        detach();
        return data()[d->size - 1];
    }

    const_reference first() const Q_DECL_NOEXCEPT
    {
        Q_ASSERT(!isEmpty());
        return data()[0];
    }

    const_reference last() const Q_DECL_NOEXCEPT
    {
        Q_ASSERT(!isEmpty());
        return data()[d->size - 1];
    }

    const_reference constFirst() const Q_DECL_NOEXCEPT { return first(); }
    const_reference constLast() const Q_DECL_NOEXCEPT { return last(); }

    value_type takeFirst() { value_type v = first(); remove(0); return v; }
    value_type takeLast() { value_type v = last(); remove(size() - 1); return v; }
    void removeFirst() { remove(0); }
    void removeLast() { remove(size() - 1); }

    reference operator[](int i)
    {
        Q_ASSERT_X(size_t(i) < d->size, "QGenericArray::operator[]", "index out of range");
        detach();
        return data()[i];
    }
    const_reference operator[](int i) const Q_DECL_NOEXCEPT { return at(i); }
    const_reference at(int i) const Q_DECL_NOEXCEPT
    {
        Q_ASSERT_X(size_t(i) < d->size, "QGenericArray::at", "index out of range");
        return data()[i];
    }

    value_type value(int i, parameter_type defaultValue) const Q_DECL_NOEXCEPT
    { return size_t(i) < d->size ? at(i) : defaultValue; }
    value_type value(int i) const Q_DECL_NOEXCEPT
    { return value(i, value_type()); }

    iterator insert(int i, parameter_type t)
    { return insert(i, 1, t); }
    iterator insert(const_iterator before, parameter_type t)
    { return insert(before, 1, t); }
    iterator insert(const_iterator before, int n, parameter_type t)
    { return insert(std::distance(constBegin(), before), n, t); }
    iterator insert(iterator before, parameter_type t)
    { return insert(before, 1, t); }
    iterator insert(iterator before, int n, parameter_type t)
    { return insert(std::distance(begin(), before), n, t); }
    void append(parameter_type t)
    { append(const_iterator(&t), const_iterator(&t) + 1); }
    void prepend(parameter_type t)
    { prepend(const_iterator(&t), const_iterator(&t) + 1); }
#ifdef Q_COMPILER_RVALUE_REFS
    template <typename Dummy = int>
    void append(value_type &&t, typename std::enable_if<std::is_reference<parameter_type>::value, Dummy>::type = 0);
#endif

    iterator insert(int i, int n, parameter_type t);
    void append(const_iterator first, const_iterator last);
    void prepend(const_iterator first, const_iterator last);
    void replace(int i, parameter_type t);
#if defined(Q_COMPILER_RVALUE_REFS) && 0
    // disabled for now
    void append(value_type &&t);
    void prepend(value_type &&t);
    void insert(int i, value_type &&t);
    void replace(int i, value_type &&t);
#endif
    void remove(int i, int n = 1);
    iterator erase(iterator first, iterator last);
    iterator erase(iterator pos) { return isEmpty() ? d.begin() : erase(pos, pos + 1); }

    void append(const QGenericArray &l) { append(l.begin(), l.end()); }

    void move(int from, int to)
    {
        Q_ASSERT_X(from >= 0 && from < size(), "QVector::move(int,int)", "'from' is out-of-range");
        Q_ASSERT_X(to >= 0 && to < size(), "QVector::move(int,int)", "'to' is out-of-range");
        if (from == to) // don't detach when no-op
            return;
        detach();
        T * const b = d->begin();
        if (from < to)
            std::rotate(b + from, b + from + 1, b + to + 1);
        else
            std::rotate(b + to, b + from, b + from + 1);
    }

    int indexOf(parameter_type t, int from = 0) const Q_DECL_NOEXCEPT;
    int lastIndexOf(parameter_type t, int from = -1) const Q_DECL_NOEXCEPT;
    bool contains(parameter_type t) const Q_DECL_NOEXCEPT
    { return indexOf(t) != -1; }
    int count(parameter_type t) const Q_DECL_NOEXCEPT;

    bool startsWith(parameter_type t) const Q_DECL_NOEXCEPT
    { return !isEmpty() && first() == t; }
    bool endsWith(parameter_type t) const Q_DECL_NOEXCEPT
    { return !isEmpty() && last() == t; }

    // STL-style functions
    void push_back(const_reference t) { append(t); }
    void push_front(const_reference t) { prepend(t); }
#ifdef Q_COMPILER_RVALUE_REFS
    void push_back(value_type &&t) { append(std::move(t)); }
    void push_front(value_type &&t) { prepend(std::move(t)); }
#endif
    void pop_back() { takeLast(); }
    void pop_front() { takeFirst(); }
    reference front() { return first(); }
    reference back() { return last(); }
    const_reference front() const { return first(); }
    const_reference back() const { return last(); }
    bool empty() const Q_DECL_NOEXCEPT { return isEmpty(); }
    void shrink_to_fit() { squeeze(); }
    void assign(const_iterator i1, const_iterator i2)
    {
        erase(begin(), end());
        d->copyAppend(i1, i2);
    }
    void assign(int n, parameter_type t)
    {
        erase(begin(), end());
        if (n)
            d->copyAppend(n, t);
    }

#if defined(Q_COMPILER_RVALUE_REFS) && defined(Q_COMPILER_VARIADIC_TEMPLATES) && 0
    // disabled for now
    template <class... Args> void emplace_back(Args && ...args);
    template <class... Args> iterator emplace(const_iterator position, Args && ...args);
#endif

    static Q_DECL_CONSTEXPR size_t max_size()
    {
        return ((std::numeric_limits<size_type>::max)() - sizeof(QArrayAllocatedData))
                / sizeof(value_type);
    }

    // QList compatibility
    void removeAt(int i) { remove(i); }
    int removeAll(const T &t)
    {
        const const_iterator ce = this->cend(), cit = std::find(this->cbegin(), ce, t);
        if (cit == ce)
            return 0;
        int index = cit - this->cbegin();
        const iterator e = end(), it = std::remove(begin() + index, e, t);
        const int result = std::distance(it, e);
        erase(it, e);
        return result;
    }
    bool removeOne(const T &t)
    {
        const int i = indexOf(t);
        if (i < 0)
            return false;
        remove(i);
        return true;
    }
    T takeAt(int i) { T t = this->at(i); remove(i); return t; }

private:
    void resize_internal(int i, Qt::Initialization);
    bool isValidIterator(const iterator &i) const
    {
        return (i <= d->end()) && (d->begin() <= i);
    }

protected:
    DataPointer d;
};

template <typename T>
inline void QGenericArray<T>::fill(parameter_type t, int newSize)
{
    if (newSize == -1)
        newSize = size();
    if (d->needsDetach() || newSize > capacity()) {
        // must allocate memory
        DataPointer detached(Data::allocate(d->detachCapacity(newSize),
                                            d->detachFlags()));
        detached->copyAppend(newSize, t);
        d.swap(detached);
    } else {
        // we're detached
        const T copy(t);
        d->assign(d.begin(), d.begin() + qMin(size(), newSize), t);
        if (newSize > size())
            d->copyAppend(newSize - size(), copy);
    }
}

template <typename T>
inline QArrayDataPointer<T> QGenericArray<T>::mid(int pos, int len) const
{
    using namespace QtPrivate;
    switch (QContainerImplHelper::mid(d.size, &pos, &len)) {
    case QContainerImplHelper::Null:
    case QContainerImplHelper::Empty:
        return QArrayDataPointer<T>();
    case QContainerImplHelper::Full:
        return d;
    case QContainerImplHelper::Subset:
        break;
    }

    // Allocate memory
    DataPointer copied(Data::allocate(len));
    copied->copyAppend(constBegin() + pos, constBegin() + pos + len);
    return copied;
}

template <typename T>
inline void QGenericArray<T>::resize_internal(int newSize, Qt::Initialization)
{
    Q_ASSERT(newSize >= 0);

    if (d->needsDetach() || newSize > capacity()) {
        // must allocate memory
        DataPointer detached(Data::allocate(d->detachCapacity(newSize),
                                            d->detachFlags()));
        if (size() && newSize) {
            detached->copyAppend(constBegin(), constBegin() + qMin(newSize, size()));
        }
        d.swap(detached);
    }

    if (newSize < size())
        d->truncate(newSize);
}

template <typename T>
inline void QGenericArray<T>::squeeze()
{
    if (d->needsDetach() || size() != capacity()) {
        // must allocate memory
        DataPointer detached(Data::allocate(size(), d->detachFlags() & ~Data::CapacityReserved));
        if (size()) {
            detached->copyAppend(constBegin(), constEnd());
        }
        d.swap(detached);
    }
}

template <typename T>
inline void QGenericArray<T>::reserve(int n)
{
    // capacity() == 0 for immutable data, so this will force a detaching below
    if (n <= capacity()) {
        if (d->flags() & Data::CapacityReserved)
            return;  // already reserved, don't shrink
        if (!d->isShared()) {
            // accept current allocation, don't shrink
            d->flags() |= Data::CapacityReserved;
            return;
        }
    }

    DataPointer detached(Data::allocate(qMax(n, size()),
                                        d->detachFlags() | Data::CapacityReserved));
    detached->copyAppend(constBegin(), constEnd());
    d.swap(detached);
}

template <typename T>
inline typename QGenericArray<T>::iterator
QGenericArray<T>::insert(int i, int n, parameter_type t)
{
    // we don't have a quick exit for n == 0
    // it's not worth wasting CPU cycles for that

    if (i < 0)
        i += size();

    const size_t newSize = size() + n;
    if (d->needsDetach() || newSize > d->allocatedCapacity()) {
        typename Data::ArrayOptions flags = d->detachFlags() | Data::GrowsForward;
        if (size_t(i) <= newSize / 4)
            flags |= Data::GrowsBackwards;

        DataPointer detached(Data::allocate(d->detachCapacity(newSize), flags));
        const_iterator where = constBegin() + i;
        detached->copyAppend(constBegin(), where);
        detached->copyAppend(n, t);
        detached->copyAppend(where, constEnd());
        d.swap(detached);
    } else {
        // we're detached and we can just move data around
        if (i == size())
            d->copyAppend(n, t);
        else
            d->insert(d.begin() + i, n, t);
    }
    return d.begin() + i;
}

template <typename T>
inline void QGenericArray<T>::append(const_iterator i1, const_iterator i2)
{
    if (i1 == i2)
        return;
    const size_t newSize = size() + std::distance(i1, i2);
    if (d->needsDetach() || newSize > d->allocatedCapacity()) {
        DataPointer detached(Data::allocate(d->detachCapacity(newSize),
                                            d->detachFlags() | Data::GrowsBackwards));
        detached->copyAppend(constBegin(), constEnd());
        detached->copyAppend(i1, i2);
        d.swap(detached);
    } else {
        // we're detached and we can just move data around
        d->copyAppend(i1, i2);
    }
}

#ifdef Q_COMPILER_RVALUE_REFS
template <typename T> template <typename Dummy>
inline void QGenericArray<T>::append(value_type &&t, typename std::enable_if<std::is_reference<parameter_type>::value, Dummy>::type)
{
    const size_t newSize = size() + 1;
    const bool isTooSmall = newSize > d->allocatedCapacity();
    const bool isOverlapping = std::addressof(*d->begin()) <= std::addressof(t)
            && std::addressof(t) < std::addressof(*d->end());
    if (isTooSmall || d->needsDetach() || Q_UNLIKELY(isOverlapping)) {
        typename Data::ArrayOptions flags = d->detachFlags();
        if (isTooSmall)
            flags |= Data::GrowsForward;
        DataPointer detached(Data::allocate(d->detachCapacity(newSize), flags));
        detached->copyAppend(constBegin(), constEnd());
        detached->moveAppend(std::addressof(t), std::addressof(t) + 1);
        d.swap(detached);
    } else {
        // we're detached and we can just move data around
        d->moveAppend(std::addressof(t), std::addressof(t) + 1);
    }
}
#endif

template <typename T>
inline void QGenericArray<T>::prepend(const_iterator i1, const_iterator i2)
{
    const size_t newSize = size() + std::distance(i1, i2);
    if (d->needsDetach() || newSize > d->allocatedCapacity()) {
        DataPointer detached(Data::allocate(d->detachCapacity(newSize),
                                            d->detachFlags() | Data::GrowsBackwards));
        detached->copyAppend(i1, i2);
        detached->copyAppend(constBegin(), constEnd());
        d.swap(detached);
    } else {
        // we're detached and we can just move data around
        if (size() == 0)
            d->copyAppend(i1, i2);
        else
            d->insert(d->data(), i1, i2);
    }
}

template <typename T>
inline void QGenericArray<T>::replace(int i, parameter_type t)
{
    Q_ASSERT_X(size_t(i) < d->size, "QGenericArray::replace", "index out of range");
    const value_type copy(t);
    data()[i] = copy;
}

template <typename T>
inline void QGenericArray<T>::remove(int i, int n)
{
    Q_ASSERT_X(size_t(i) + size_t(n) <= d->size, "QGenericArray::remove", "index out of range");
    Q_ASSERT_X(n >= 0, "QGenericArray::remove", "invalid count");

    if (n == 0)
        return;

    const size_t newSize = size() - n;
    if (d->needsDetach() ||
            ((d->flags() & Data::CapacityReserved) == 0
             && newSize < d->allocatedCapacity()/2)) {
        // allocate memory
        DataPointer detached(Data::allocate(d->detachCapacity(newSize),
                             d->detachFlags() & ~(Data::GrowsBackwards | Data::GrowsForward)));
        const_iterator where = constBegin() + i;
        if (newSize) {
            detached->copyAppend(constBegin(), where);
            detached->copyAppend(where + n, constEnd());
        }
        d.swap(detached);
    } else {
        // we're detached and we can just move data around
        d->erase(d->begin() + i, d->begin() + i + n);
    }
}

template <typename T>
inline typename QGenericArray<T>::iterator
QGenericArray<T>::erase(iterator i1, iterator i2)
{
    // these asserts must not detach
    Q_ASSERT(i1 >= d->begin());
    Q_ASSERT(i2 >= i1);
    Q_ASSERT(i2 <= d->end());

    // d.begin() so we don't detach just yet
    int i = std::distance(d.begin(), i1);
    int n = std::distance(i1, i2);
    remove(i, n);

    return begin() + i;
}

template <typename T>
inline int QGenericArray<T>::indexOf(parameter_type t, int from) const Q_DECL_NOEXCEPT
{
    if (from < 0)
        from = qMax(from + size(), 0);
    if (from < size()) {
        const_pointer n = d->begin() + from - 1;
        const_pointer e = d->end();
        while (++n != e)
            if (*n == t)
                return n - d->begin();
    }
    return -1;
}

template <typename T>
inline int QGenericArray<T>::lastIndexOf(parameter_type t, int from) const Q_DECL_NOEXCEPT
{
    if (from < 0)
        from += size();
    else if (from >= size())
        from = size() - 1;
    if (from >= 0) {
        const_pointer b = d->begin();
        const_pointer n = d->begin() + from + 1;
        while (n != b) {
            if (*--n == t)
                return n - b;
        }
    }
    return -1;
}

template <typename T>
inline int QGenericArray<T>::count(parameter_type t) const Q_DECL_NOEXCEPT
{
    return int(std::count(&*cbegin(), &*cend(), t));
}

QT_END_NAMESPACE

QT_END_HEADER

#endif // QGENERICARRAY_H
