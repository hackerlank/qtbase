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

#ifndef QARRAYDATA_H
#define QARRAYDATA_H

#include <QtCore/qpair.h>
#include <QtCore/qatomic.h>
#include <string.h>

QT_BEGIN_NAMESPACE

template <class T> struct QTypedArrayData;

struct QArrayAllocatedData;
struct QArrayForeignData;
struct Q_CORE_EXPORT QArrayData
{
    enum ArrayOption {
        RawDataType          = 0x0001,  //!< this class is really a QArrayRawData
        AllocatedDataType    = 0x0002,  //!< this class is really a QArrayAllocatedData
        ForeignDataType      = 0x0004,  //!< this class is really a QArrayForeignData
        DataTypeBits         = 0x000f,

        Unsharable           = 0x0010,  //!< always do deep copies, never shallow copies / implicit sharing
        CapacityReserved     = 0x0020,  //!< the capacity was reserved by the user, try to keep it
        GrowsForward         = 0x0040,  //!< allocate with eyes towards growing through append()
        GrowsBackwards       = 0x0040,  //!< allocate with eyes towards growing through prepend()
        MutableData          = 0x0100,  //!< the data can be changed; doesn't say anything about the header
        ImmutableHeader      = 0x0200,  //!< the header is static, it can't be changed

        /// this option is used by the Q_ARRAY_LITERAL and similar macros
        StaticDataFlags = ImmutableHeader,
        /// this option is used by the allocate() function
        DefaultAllocationFlags = MutableData,
        /// this option is used by the prepareRawData() function
        DefaultRawFlags = 0
    };
    Q_DECLARE_FLAGS(ArrayOptions, ArrayOption)

    uint flags;
    int size;       // ### move to the main class body?
    qptrdiff offset; // in bytes from beginning of header
    // size is 12 / 16 bytes

    inline size_t allocatedCapacity();
    inline size_t constAllocatedCapacity() const;
    inline QBasicAtomicInt &refCounter();
    inline int refCounterValue() const;
    inline QArrayAllocatedData *asAllocatedData();
    inline const QArrayAllocatedData *asAllocatedData() const;
    inline QArrayForeignData *asForeignData();
    inline const QArrayForeignData *asForeignData() const;

    /// Returns true if sharing took place
    bool ref()
    {
#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
        if (!isSharable())
            return false;
#endif
        if (!isStatic())
            refCounter().ref();
        return true;
    }

    /// Returns false if deallocation is necessary
    bool deref()
    {
        if (isStatic())
            return true;
        return refCounter().deref();
    }

    void *data()
    {
        Q_ASSERT(size == 0
                || offset < 0 || size_t(offset) >= sizeof(QArrayData));
        return reinterpret_cast<char *>(this) + offset;
    }

    const void *data() const
    {
        Q_ASSERT(size == 0
                || offset < 0 || size_t(offset) >= sizeof(QArrayData));
        return reinterpret_cast<const char *>(this) + offset;
    }

    // This refers to array data mutability, not "header data" represented by
    // data members in QArrayData. Shared data (array and header) must still
    // follow COW principles.
    bool isMutable() const
    {
        return flags & MutableData;
    }

    bool isStatic() const
    {
        return flags & ImmutableHeader;
    }

    bool isShared() const
    {
#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
        return isStatic() ? isSharable() : refCounterValue() != 1;
#else
        return isStatic() || refCounterValue() != 1;
#endif
    }

#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
    bool isSharable() const
    {
        return (flags & Unsharable) == 0;
    }

    void setSharable(bool sharable)
    {
        if (sharable)
            flags &= uint(~Unsharable);
        else
            flags |= Unsharable;
    }
#endif

    // Returns true if a detach is necessary before modifying the data
    // This method is intentionally not const: if you want to know whether
    // detaching is necessary, you should be in a non-const function already
    bool needsDetach()
    {
        // requires two conditionals
        return !isMutable() || isShared();
    }

    inline size_t detachCapacity(size_t newSize) const;

    ArrayOptions detachFlags() const
    {
        ArrayOptions result = DefaultAllocationFlags;
        if (flags & CapacityReserved)
            result |= CapacityReserved;
        return result;
    }

    ArrayOptions cloneFlags() const
    {
        ArrayOptions result = DefaultAllocationFlags;
        if (flags & CapacityReserved)
            result |= CapacityReserved;
        return result;
    }

#if defined(Q_CC_GNU)
    __attribute__((__malloc__))
#endif
    static void *allocate(QArrayData **pdata, size_t objectSize, size_t alignment,
            size_t capacity, ArrayOptions options = DefaultAllocationFlags)
        Q_DECL_NOTHROW Q_REQUIRED_RESULT;
    static QPair<QArrayData *, void *> reallocateUnaligned(QArrayData *data, void *dataPointer,
            size_t objectSize, size_t newCapacity, ArrayOptions newOptions = DefaultAllocationFlags)
        Q_DECL_NOTHROW Q_REQUIRED_RESULT;
    static QArrayData *prepareRawData(ArrayOptions options = ArrayOptions(RawDataType))
        Q_DECL_NOTHROW Q_REQUIRED_RESULT;
    static QArrayData *prepareForeignData(ArrayOptions options = ArrayOptions(ForeignDataType))
        Q_DECL_NOTHROW Q_REQUIRED_RESULT;
    static void deallocate(QArrayData *data, size_t objectSize,
            size_t alignment) Q_DECL_NOTHROW;

    static const QArrayData shared_null[2];
    static QArrayData *sharedNull() Q_DECL_NOTHROW { return const_cast<QArrayData*>(shared_null); }
    static void *sharedNullData()
    {
        QArrayData *const null = const_cast<QArrayData *>(&shared_null[1]);
        Q_ASSERT(sharedNull()->data() == null);
        return null;
    }
};

struct QArrayRawData : public QArrayData
{
    QBasicAtomicInt ref_;
    // 4 bytes tail-padding on 64-bit systems
    // size is 16 / 24 bytes
};

struct QArrayAllocatedData : public QArrayData
{
    QBasicAtomicInt ref_;
    uint alloc;
    // size is 16 / 24 bytes
};

struct QArrayForeignData : public QArrayData
{
    QBasicAtomicInt ref_;
    // -- 4 bytes padding on 64-bit systems --
    void *token;
    void (*notifyFunction)(void *);
    // size is 24 / 40 bytes
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QArrayData::ArrayOptions)

inline size_t QArrayData::detachCapacity(size_t newSize) const
{
    if ((flags & AllocatedDataType) && (flags & CapacityReserved) && newSize < asAllocatedData()->alloc)
        return asAllocatedData()->alloc;
    return newSize;
}

inline QArrayAllocatedData *QArrayData::asAllocatedData()
{
    Q_ASSERT(flags & AllocatedDataType);
    return static_cast<QArrayAllocatedData *>(this);
}

inline const QArrayAllocatedData *QArrayData::asAllocatedData() const
{
    Q_ASSERT(flags & AllocatedDataType);
    return static_cast<const QArrayAllocatedData *>(this);
}

inline QArrayForeignData *QArrayData::asForeignData()
{
    Q_ASSERT(flags & ForeignDataType);
    return static_cast<QArrayForeignData *>(this);
}

inline const QArrayForeignData *QArrayData::asForeignData() const
{
    Q_ASSERT(flags & ForeignDataType);
    return static_cast<const QArrayForeignData *>(this);
}

inline size_t QArrayData::constAllocatedCapacity() const
{
    return flags & AllocatedDataType ? asAllocatedData()->alloc : 0;
}

inline size_t QArrayData::allocatedCapacity()
{
    return constAllocatedCapacity();
}

inline QBasicAtomicInt &QArrayData::refCounter()
{
    QBasicAtomicInt &allocated = static_cast<QArrayAllocatedData *>(this)->ref_;
    QBasicAtomicInt &foreign = static_cast<QArrayForeignData *>(this)->ref_;
    QBasicAtomicInt &raw = static_cast<QArrayRawData *>(this)->ref_;
    Q_ASSERT(&allocated == &foreign && &allocated == &raw);
    Q_ASSERT(!isStatic());
    Q_UNUSED(allocated);
    Q_UNUSED(foreign);
    return raw;
}

inline int QArrayData::refCounterValue() const
{
    const QBasicAtomicInt &allocated = static_cast<const QArrayAllocatedData *>(this)->ref_;
    const QBasicAtomicInt &foreign = static_cast<const QArrayForeignData *>(this)->ref_;
    const QBasicAtomicInt &raw = static_cast<const QArrayRawData *>(this)->ref_;
    Q_ASSERT(&allocated == &foreign && &allocated == &raw);
    Q_ASSERT(!isStatic());
    Q_UNUSED(allocated);
    Q_UNUSED(foreign);
    return raw.load();
}

template <class T, size_t N>
struct QStaticArrayData
{
    // static arrays are of type RawDataType
    QArrayData header;
    T data[N];
};

// Support for returning QArrayDataPointer<T> from functions
template <class T>
struct QArrayDataPointerRef
{
    QTypedArrayData<T> *ptr;
    T *data;
    uint size;
};

template <class T>
struct QTypedArrayData
    : QArrayData
{
#ifdef QT_STRICT_ITERATORS
    class iterator {
    public:
        T *i;
        typedef std::random_access_iterator_tag  iterator_category;
        typedef int difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;

        inline iterator() : i(Q_NULLPTR) {}
        inline iterator(T *n) : i(n) {}
        inline iterator(const iterator &o): i(o.i){} // #### Qt 6: remove, the implicit version is fine
        inline T &operator*() const { return *i; }
        inline T *operator->() const { return i; }
        inline T &operator[](int j) const { return *(i + j); }
        inline bool operator==(const iterator &o) const { return i == o.i; }
        inline bool operator!=(const iterator &o) const { return i != o.i; }
        inline bool operator<(const iterator& other) const { return i < other.i; }
        inline bool operator<=(const iterator& other) const { return i <= other.i; }
        inline bool operator>(const iterator& other) const { return i > other.i; }
        inline bool operator>=(const iterator& other) const { return i >= other.i; }
        inline iterator &operator++() { ++i; return *this; }
        inline iterator operator++(int) { T *n = i; ++i; return n; }
        inline iterator &operator--() { i--; return *this; }
        inline iterator operator--(int) { T *n = i; i--; return n; }
        inline iterator &operator+=(int j) { i+=j; return *this; }
        inline iterator &operator-=(int j) { i-=j; return *this; }
        inline iterator operator+(int j) const { return iterator(i+j); }
        inline iterator operator-(int j) const { return iterator(i-j); }
        inline int operator-(iterator j) const { return i - j.i; }
        inline operator T*() const { return i; }
    };
    friend class iterator;

    class const_iterator {
    public:
        const T *i;
        typedef std::random_access_iterator_tag  iterator_category;
        typedef int difference_type;
        typedef T value_type;
        typedef const T *pointer;
        typedef const T &reference;

        inline const_iterator() : i(Q_NULLPTR) {}
        inline const_iterator(const T *n) : i(n) {}
        inline const_iterator(const const_iterator &o): i(o.i) {} // #### Qt 6: remove, the default version is fine
        inline explicit const_iterator(const iterator &o): i(o.i) {}
        inline const T &operator*() const { return *i; }
        inline const T *operator->() const { return i; }
        inline const T &operator[](int j) const { return *(i + j); }
        inline bool operator==(const const_iterator &o) const { return i == o.i; }
        inline bool operator!=(const const_iterator &o) const { return i != o.i; }
        inline bool operator<(const const_iterator& other) const { return i < other.i; }
        inline bool operator<=(const const_iterator& other) const { return i <= other.i; }
        inline bool operator>(const const_iterator& other) const { return i > other.i; }
        inline bool operator>=(const const_iterator& other) const { return i >= other.i; }
        inline const_iterator &operator++() { ++i; return *this; }
        inline const_iterator operator++(int) { const T *n = i; ++i; return n; }
        inline const_iterator &operator--() { i--; return *this; }
        inline const_iterator operator--(int) { const T *n = i; i--; return n; }
        inline const_iterator &operator+=(int j) { i+=j; return *this; }
        inline const_iterator &operator-=(int j) { i-=j; return *this; }
        inline const_iterator operator+(int j) const { return const_iterator(i+j); }
        inline const_iterator operator-(int j) const { return const_iterator(i-j); }
        inline int operator-(const_iterator j) const { return i - j.i; }
        inline operator const T*() const { return i; }
    };
    friend class const_iterator;
#else
    typedef T* iterator;
    typedef const T* const_iterator;
#endif

    T *data() { return static_cast<T *>(QArrayData::data()); }
    const T *data() const { return static_cast<const T *>(QArrayData::data()); }

    iterator begin(iterator = iterator()) { return data(); }
    iterator end(iterator = iterator()) { return data() + size; }
    const_iterator begin(const_iterator = const_iterator()) const { return data(); }
    const_iterator end(const_iterator = const_iterator()) const { return data() + size; }
    const_iterator constBegin(const_iterator = const_iterator()) const { return data(); }
    const_iterator constEnd(const_iterator = const_iterator()) const { return data() + size; }

    class AlignmentDummy { QArrayData header; T data; };

    static QPair<QTypedArrayData *, T *> allocate(size_t capacity,
            ArrayOptions options = DefaultAllocationFlags) Q_REQUIRED_RESULT
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        QArrayData *d;
        void *result = QArrayData::allocate(&d, sizeof(T),
                    Q_ALIGNOF(AlignmentDummy), capacity, options);
#if (defined(Q_CC_GNU) && Q_CC_GNU >= 407) || QT_HAS_BUILTIN(__builtin_assume_aligned)
        result = __builtin_assume_aligned(result, Q_ALIGNOF(AlignmentDummy));
#endif
        return qMakePair(static_cast<QTypedArrayData *>(d), static_cast<T *>(result));
    }

    static QPair<QTypedArrayData *, T *>
    reallocateUnaligned(QTypedArrayData *data, T *dataPointer, size_t capacity,
            ArrayOptions options = DefaultAllocationFlags)
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        QPair<QArrayData *, void *> pair =
                QArrayData::reallocateUnaligned(data, dataPointer, sizeof(T), capacity, options);
        return qMakePair(static_cast<QTypedArrayData *>(pair.first), static_cast<T *>(pair.second));
    }

    static void deallocate(QArrayData *data)
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        QArrayData::deallocate(data, sizeof(T), Q_ALIGNOF(AlignmentDummy));
    }

    static QArrayDataPointerRef<T> fromRawData(const T *data, size_t n,
            ArrayOptions options = DefaultRawFlags)
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        QArrayDataPointerRef<T> result = {
            static_cast<QTypedArrayData *>(prepareRawData(options)), const_cast<T *>(data), uint(n)
        };
        if (result.ptr) {
            Q_ASSERT(!result.ptr->isShared()); // No shared empty, please!
            result.ptr->offset = reinterpret_cast<const char *>(data)
                - reinterpret_cast<const char *>(result.ptr);
            result.ptr->size = int(n);
        }
        return result;
    }

    static QTypedArrayData *sharedNull() Q_DECL_NOTHROW
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        return static_cast<QTypedArrayData *>(QArrayData::sharedNull());
    }

    static QTypedArrayData *sharedEmpty()
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        return allocate(/* capacity */ 0);
    }
#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
#endif

    static T *sharedNullData()
    {
        Q_STATIC_ASSERT(sizeof(QTypedArrayData) == sizeof(QArrayData));
        return static_cast<T *>(QArrayData::sharedNullData());
    }
};

#define Q_STATIC_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size, offset) \
    { QArrayData::StaticDataFlags, size, offset } \
    /**/

#define Q_STATIC_ARRAY_DATA_HEADER_INITIALIZER(type, size) \
    Q_STATIC_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(size,\
        ((sizeof(QArrayData) + (Q_ALIGNOF(type) - 1)) & ~(Q_ALIGNOF(type) - 1) )) \
    /**/

////////////////////////////////////////////////////////////////////////////////
//  Q_ARRAY_LITERAL

// The idea here is to place a (read-only) copy of header and array data in an
// mmappable portion of the executable (typically, .rodata section). This is
// accomplished by hiding a static const instance of QStaticArrayData, which is
// POD.

#if defined(Q_COMPILER_VARIADIC_MACROS)
#if defined(Q_COMPILER_LAMBDA)
// Hide array inside a lambda
#define Q_ARRAY_LITERAL(Type, ...)                                              \
    ([]() -> QArrayDataPointerRef<Type> {                                       \
            /* MSVC 2010 Doesn't support static variables in a lambda, but */   \
            /* happily accepts them in a static function of a lambda-local */   \
            /* struct :-) */                                                    \
            struct StaticWrapper {                                              \
                static QArrayDataPointerRef<Type> get()                         \
                {                                                               \
                    Q_ARRAY_LITERAL_IMPL(Type, __VA_ARGS__)                     \
                    return ref;                                                 \
                }                                                               \
            };                                                                  \
            return StaticWrapper::get();                                        \
        }())                                                                    \
    /**/
#endif
#endif // defined(Q_COMPILER_VARIADIC_MACROS)

#if defined(Q_ARRAY_LITERAL)
#define Q_ARRAY_LITERAL_IMPL(Type, ...)                                         \
    union { Type type_must_be_POD; } dummy; Q_UNUSED(dummy)                     \
                                                                                \
    /* Portable compile-time array size computation */                          \
    Type data[] = { __VA_ARGS__ }; Q_UNUSED(data)                               \
    enum { Size = sizeof(data) / sizeof(data[0]) };                             \
                                                                                \
    static const QStaticArrayData<Type, Size> literal = {                       \
        Q_STATIC_ARRAY_DATA_HEADER_INITIALIZER(Type, Size), { __VA_ARGS__ } };  \
                                                                                \
    QArrayDataPointerRef<Type> ref =                                            \
        { static_cast<QTypedArrayData<Type> *>(                                 \
            const_cast<QArrayData *>(&literal.header)),                         \
          const_cast<Type *>(literal.data),                                     \
          Size };                                                               \
    /**/
#else
// As a fallback, memory is allocated and data copied to the heap.

// The fallback macro does NOT use variadic macros and does NOT support
// variable number of arguments. It is suitable for char arrays.

namespace QtPrivate {
    template <class T, size_t N>
    inline QArrayDataPointerRef<T> qMakeArrayLiteral(const T (&array)[N])
    {
        union { T type_must_be_POD; } dummy; Q_UNUSED(dummy)

        QArrayDataPointerRef<T> result = { QTypedArrayData<T>::allocate(N) };
        Q_CHECK_PTR(result.ptr);

        ::memcpy(result.ptr->data(), array, N * sizeof(T));
        result.ptr->size = N;

        return result;
    }
}

#define Q_ARRAY_LITERAL(Type, Array) \
    QT_PREPEND_NAMESPACE(QtPrivate::qMakeArrayLiteral)<Type>( Array )
#endif // !defined(Q_ARRAY_LITERAL)

namespace QtPrivate {
struct Q_CORE_EXPORT QContainerImplHelper
{
    enum CutResult { Null, Empty, Full, Subset };
    static CutResult mid(int originalLength, int *position, int *length);
};
}

QT_END_NAMESPACE

#endif // include guard
