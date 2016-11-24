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

#ifndef QARRAYDATAOPS_H
#define QARRAYDATAOPS_H

#include <QtCore/qarraydata.h>

#include <new>
#include <string.h>

#if __cplusplus >= 201103L
#  include <type_traits>
#  define Q_IS_POD(T)       std::is_pod<T>::value
#else
#  define Q_IS_POD(T)       false
#endif

QT_BEGIN_NAMESPACE

template <class T> struct QArrayDataPointer;

namespace QtPrivate {

template <class T>
struct QPodArrayOps
        : public QArrayDataPointer<T>
{
    typedef typename QArrayDataPointer<T>::parameter_type parameter_type;

    void appendInitialize(size_t newSize)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(newSize > uint(this->size));
        Q_ASSERT(newSize <= this->allocatedCapacity());

        ::memset(this->end(), 0, (newSize - this->size) * sizeof(T));
        this->size = uint(newSize);
    }

    void copyAppend(const T *b, const T *e)
    {
        Q_ASSERT(this->isMutable() || b == e);
        Q_ASSERT(!this->isShared() || b == e);
        Q_ASSERT(b <= e);
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        ::memcpy(this->end(), b, (e - b) * sizeof(T));
        this->size += e - b;
    }

    void moveAppend(T *b, T *e)
    { copyAppend(b, e); }

    void copyAppend(size_t n, parameter_type t)
    {
        Q_ASSERT(this->isMutable() || n == 0);
        Q_ASSERT(!this->isShared() || n == 0);
        Q_ASSERT(n <= uint(this->allocatedCapacity() - this->size));

        T *iter = this->end();
        const T *const end = iter + n;
        for (; iter != end; ++iter)
            *iter = t;
        this->size += uint(n);
    }

    void truncate(size_t newSize)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(newSize < size_t(this->size));

        this->size = uint(newSize);
    }

    void destroyAll() // Call from destructors, ONLY!
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(this->refCounterValue() == 0);

        // As this is to be called only from destructor, it doesn't need to be
        // exception safe; size not updated.
    }

    void insert(T *where, const T *b, const T *e)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(where >= this->begin() && where < this->end()); // Use copyAppend at end
        Q_ASSERT(b <= e);
        Q_ASSERT(e <= where || b > this->end()); // No overlap
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        ::memmove(where + (e - b), where, (static_cast<const T*>(this->end()) - where) * sizeof(T));
        ::memcpy(where, b, (e - b) * sizeof(T));
        this->size += (e - b);
    }

    void insert(T *where, size_t n, parameter_type t)
    {
        Q_ASSERT(!this->isShared());
        Q_ASSERT(where >= this->begin() && where < this->end()); // Use copyAppend at end
        Q_ASSERT(this->allocatedCapacity() - this->size >= n);

        ::memmove(where + n, where, (static_cast<const T*>(this->end()) - where) * sizeof(T));
        this->size += uint(n); // PODs can't throw on copy
        while (n--)
            *where++ = t;
    }

    void erase(T *b, T *e)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(b < e);
        Q_ASSERT(b >= this->begin() && b < this->end());
        Q_ASSERT(e > this->begin() && e <= this->end());

        ::memmove(b, e, (static_cast<T *>(this->end()) - e) * sizeof(T));
        this->size -= (e - b);
    }

    void assign(T *b, T *e, parameter_type t)
    {
        Q_ASSERT(b <= e);
        Q_ASSERT(b >= this->begin() && e <= this->end());

        while (b != e)
            ::memcpy(b++, &t, sizeof(T));
    }

    bool compare(const T *begin1, const T *begin2, size_t n) const
    {
        return ::memcmp(begin1, begin2, n * sizeof(T)) == 0;
    }
};

template <class T>
struct QGenericArrayOps
        : public QArrayDataPointer<T>
{
    typedef typename QArrayDataPointer<T>::parameter_type parameter_type;

    void appendInitialize(size_t newSize)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(newSize > uint(this->size));
        Q_ASSERT(newSize <= this->allocatedCapacity());

        T *const b = this->begin();
        do {
            new (b + this->size) T();
        } while (uint(++this->size) != newSize);
    }

    void copyAppend(const T *b, const T *e)
    {
        Q_ASSERT(this->isMutable() || b == e);
        Q_ASSERT(!this->isShared() || b == e);
        Q_ASSERT(b <= e);
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        T *iter = this->end();
        for (; b != e; ++iter, ++b) {
            new (iter) T(*b);
            ++this->size;
        }
    }

    void moveAppend(T *b, T *e)
    {
#ifdef Q_COMPILER_RVALUE_REFS
        Q_ASSERT(this->isMutable() || b == e);
        Q_ASSERT(!this->isShared() || b == e);
        Q_ASSERT(b <= e);
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        T *iter = this->end();
        for (; b != e; ++iter, ++b) {
            new (iter) T(std::move(*b));
            ++this->size;
        }
#else
        copyAppend(b, e);
#endif
    }

    void copyAppend(size_t n, parameter_type t)
    {
        Q_ASSERT(this->isMutable() || n == 0);
        Q_ASSERT(!this->isShared() || n == 0);
        Q_ASSERT(n <= size_t(this->allocatedCapacity() - this->size));

        T *iter = this->end();
        const T *const end = iter + n;
        for (; iter != end; ++iter) {
            new (iter) T(t);
            ++this->size;
        }
    }

    void truncate(size_t newSize)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(newSize < size_t(this->size));

        const T *const b = this->begin();
        do {
            (b + --this->size)->~T();
        } while (uint(this->size) != newSize);
    }

    void destroyAll() // Call from destructors, ONLY
    {
        Q_ASSERT(this->isMutable());
        // As this is to be called only from destructor, it doesn't need to be
        // exception safe; size not updated.

        Q_ASSERT(this->refCounterValue() == 0);

        const T *const b = this->begin();
        const T *i = this->end();

        while (i != b)
            (--i)->~T();
    }

    void insert(T *where, const T *b, const T *e)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(where >= this->begin() && where < this->end()); // Use copyAppend at end
        Q_ASSERT(b <= e);
        Q_ASSERT(e <= where || b > this->end()); // No overlap
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        // Array may be truncated at where in case of exceptions

        T *const end = this->end();
        const T *readIter = end;
        T *writeIter = end + (e - b);

        const T *const step1End = where + qMax(e - b, end - where);

        struct Destructor
        {
            Destructor(T *&it)
                : iter(&it)
                , end(it)
            {
            }

            void commit()
            {
                iter = &end;
            }

            ~Destructor()
            {
                for (; *iter != end; --*iter)
                    (*iter)->~T();
            }

            T **iter;
            T *end;
        } destroyer(writeIter);

        // Construct new elements in array
        do {
            --readIter, --writeIter;
            new (writeIter) T(*readIter);
        } while (writeIter != step1End);

        while (writeIter != end) {
            --e, --writeIter;
            new (writeIter) T(*e);
        }

        destroyer.commit();
        this->size += destroyer.end - end;

        // Copy assign over existing elements
        while (readIter != where) {
            --readIter, --writeIter;
            *writeIter = *readIter;
        }

        while (writeIter != where) {
            --e, --writeIter;
            *writeIter = *e;
        }
    }

    void insert(T *where, size_t n, parameter_type t)
    {
        Q_ASSERT(!this->isShared());
        Q_ASSERT(where >= this->begin() && where <= this->end());
        Q_ASSERT(this->allocatedCapacity() - this->size >= n);

        // Array may be truncated at where in case of exceptions
        T *const end = this->end();
        const T *readIter = end;
        T *writeIter = end + n;

        const T *const step1End = where + qMax<size_t>(n, end - where);

        struct Destructor
        {
            Destructor(T *&it)
                : iter(&it)
                , end(it)
            {
            }

            void commit()
            {
                iter = &end;
            }

            ~Destructor()
            {
                for (; *iter != end; --*iter)
                    (*iter)->~T();
            }

            T **iter;
            T *end;
        } destroyer(writeIter);

        // Construct new elements in array
        do {
            --readIter, --writeIter;
            new (writeIter) T(*readIter);
        } while (writeIter != step1End);

        while (writeIter != end) {
            --n, --writeIter;
            new (writeIter) T(t);
        }

        destroyer.commit();
        this->size += destroyer.end - end;

        // Copy assign over existing elements
        while (readIter != where) {
            --readIter, --writeIter;
            *writeIter = *readIter;
        }

        while (writeIter != where) {
            --n, --writeIter;
            *writeIter = t;
        }
    }

    void erase(T *b, T *e)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(b < e);
        Q_ASSERT(b >= this->begin() && b < this->end());
        Q_ASSERT(e > this->begin() && e <= this->end());

        const T *const end = this->end();

        // move (by assignment) the elements from e to end
        // onto b to the new end
        while (e != end) {
            *b = *e;
            ++b, ++e;
        }

        // destroy the final elements at the end
        // here, b points to the new end and e to the actual end
        do {
            (--e)->~T();
            --this->size;
        } while (e != b);
    }

    void assign(T *b, T *e, parameter_type t)
    {
        Q_ASSERT(b <= e);
        Q_ASSERT(b >= this->begin() && e <= this->end());

        while (b != e)
            *b++ = t;
    }

    bool compare(const T *begin1, const T *begin2, size_t n) const
    {
        const T *end1 = begin1 + n;
        while (begin1 != end1) {
            if (*begin1 == *begin2)
                ++begin1, ++begin2;
            else
                return false;
        }
        return true;
    }
};

template <class T>
struct QMovableArrayOps
    : QGenericArrayOps<T>
{
    // using QGenericArrayOps<T>::appendInitialize;
    // using QGenericArrayOps<T>::copyAppend;
    // using QGenericArrayOps<T>::truncate;
    // using QGenericArrayOps<T>::destroyAll;
    typedef typename QGenericArrayOps<T>::parameter_type parameter_type;

    void insert(T *where, const T *b, const T *e)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(where >= this->begin() && where < this->end()); // Use copyAppend at end
        Q_ASSERT(b <= e);
        Q_ASSERT(e <= where || b > this->end()); // No overlap
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        // Provides strong exception safety guarantee,
        // provided T::~T() nothrow

        struct ReversibleDisplace
        {
            ReversibleDisplace(T *start, T *finish, size_t diff)
                : begin(start)
                , end(finish)
                , displace(diff)
            {
                ::memmove(static_cast<void *>(begin + displace), static_cast<void *>(begin),
                          (end - begin) * sizeof(T));
            }

            void commit() { displace = 0; }

            ~ReversibleDisplace()
            {
                if (displace)
                    ::memmove(static_cast<void *>(begin), static_cast<void *>(begin + displace),
                              (end - begin) * sizeof(T));
            }

            T *const begin;
            T *const end;
            size_t displace;

        } displace(where, this->end(), size_t(e - b));

        struct CopyConstructor
        {
            CopyConstructor(T *w) : where(w) {}

            void copy(const T *src, const T *const srcEnd)
            {
                n = 0;
                for (; src != srcEnd; ++src) {
                    new (where + n) T(*src);
                    ++n;
                }
                n = 0;
            }

            ~CopyConstructor()
            {
                while (n)
                    where[--n].~T();
            }

            T *const where;
            size_t n;
        } copier(where);

        copier.copy(b, e);
        displace.commit();
        this->size += (e - b);
    }

    void insert(T *where, size_t n, parameter_type t)
    {
        Q_ASSERT(!this->isShared());
        Q_ASSERT(where >= this->begin() && where <= this->end());
        Q_ASSERT(this->allocatedCapacity() - this->size >= n);

        // Provides strong exception safety guarantee,
        // provided T::~T() nothrow

        struct ReversibleDisplace
        {
            ReversibleDisplace(T *start, T *finish, size_t diff)
                : begin(start)
                , end(finish)
                , displace(diff)
            {
                ::memmove(static_cast<void *>(begin + displace), static_cast<void *>(begin),
                          (end - begin) * sizeof(T));
            }

            void commit() { displace = 0; }

            ~ReversibleDisplace()
            {
                if (displace)
                    ::memmove(static_cast<void *>(begin), static_cast<void *>(begin + displace),
                              (end - begin) * sizeof(T));
            }

            T *const begin;
            T *const end;
            size_t displace;

        } displace(where, this->end(), n);

        struct CopyConstructor
        {
            CopyConstructor(T *w) : where(w) {}

            void copy(size_t count, parameter_type proto)
            {
                n = 0;
                while (count--) {
                    new (where + n) T(proto);
                    ++n;
                }
                n = 0;
            }

            ~CopyConstructor()
            {
                while (n)
                    where[--n].~T();
            }

            T *const where;
            size_t n;
        } copier(where);

        copier.copy(n, t);
        displace.commit();
        this->size += int(n);
    }

    void erase(T *b, T *e)
    {
        Q_ASSERT(this->isMutable());
        Q_ASSERT(b < e);
        Q_ASSERT(b >= this->begin() && b < this->end());
        Q_ASSERT(e > this->begin() && e <= this->end());

        struct Mover
        {
            Mover(T *&start, const T *finish, uint &sz)
                : destination(start)
                , source(start)
                , n(finish - start)
                , size(sz)
            {
            }

            ~Mover()
            {
                ::memmove(static_cast<void *>(destination), static_cast<const void *>(source), n * sizeof(T));
                size -= (source - destination);
            }

            T *&destination;
            const T *const source;
            size_t n;
            uint &size;
        } mover(e, this->end(), this->size);

        // destroy the elements we're erasing
        do {
            // Exceptions or not, dtor called once per instance
            (--e)->~T();
        } while (e != b);
    }

    void moveAppend(T *b, T *e)
    {
#ifdef Q_COMPILER_RVALUE_REFS
        Q_ASSERT(this->isMutable());
        Q_ASSERT(!this->isShared());
        Q_ASSERT(b <= e);
        Q_ASSERT(e <= this->begin() || b > this->end()); // No overlap
        Q_ASSERT(size_t(e - b) <= this->allocatedCapacity() - this->size);

        // Provides strong exception safety guarantee,
        // provided T::~T() nothrow

        struct CopyConstructor
        {
            CopyConstructor(T *w) : where(w) {}

            void copy(const T *src, const T *const srcEnd)
            {
                n = 0;
                for (; src != srcEnd; ++src) {
                    new (where + n) T(std::move(*src));
                    ++n;
                }
                n = 0;
            }

            ~CopyConstructor()
            {
                while (n)
                    where[--n].~T();
            }

            T *const where;
            size_t n;
        } copier(this->end());

        copier.copy(b, e);
        this->size += (e - b);
#else
        copyAppend(b, e);
#endif
    }
};

template <class T, class = void>
struct QArrayOpsSelector
{
    typedef QGenericArrayOps<T> Type;
};

template <class T>
struct QArrayOpsSelector<T,
    typename QEnableIf<
        (!QTypeInfo<T>::isComplex && !QTypeInfo<T>::isStatic) || Q_IS_POD(T)
    >::Type>
{
    typedef QPodArrayOps<T> Type;
};

template <class T>
struct QArrayOpsSelector<T,
    typename QEnableIf<
        QTypeInfo<T>::isComplex && !QTypeInfo<T>::isStatic && !Q_IS_POD(T)
    >::Type>
{
    typedef QMovableArrayOps<T> Type;
};

} // namespace QtPrivate

template <class T>
struct QArrayDataOps
    : QtPrivate::QArrayOpsSelector<T>::Type
{
};

QT_END_NAMESPACE

#undef Q_IS_POD

#endif // include guard
