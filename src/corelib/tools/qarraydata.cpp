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

#include <QtCore/qarraydata.h>
#include <QtCore/private/qnumeric_p.h>
#include <QtCore/private/qtools_p.h>

#include <stdlib.h>

QT_BEGIN_NAMESPACE

const QArrayData::SharedNull QArrayData::shared_null = {
    { QArrayData::StaticDataFlags }, // shared null
    { /* zero initialized terminator */ } };
#ifdef Q_DECL_ALIGN
Q_STATIC_ASSERT(sizeof(QArrayData::SharedNull) >= 32);
Q_STATIC_ASSERT(Q_ALIGNOF(QArrayData::SharedNull) >= 16);
#endif
#ifdef Q_PROCESSOR_X86
Q_STATIC_ASSERT(sizeof(QArrayAllocatedData) == 16);
#endif

#ifdef Q_DECL_ALIGN
Q_DECL_ALIGN(16)
#endif
const QArrayData QArrayData::shared_static_data = {
    QArrayData::StaticDataFlags
};

static const QArrayData qt_array_empty =
    { QArrayData::StaticDataFlags };

#ifndef QT_NO_UNSHARABLE_CONTAINERS
static const QArrayData qt_array_unsharable_empty =
    { QArrayData::StaticDataFlags | QArrayData::Unsharable };
#endif

enum {
    AlignOfDouble = Q_ALIGNOF(double),
    AlignOfPointer = Q_ALIGNOF(void*),
#ifdef __GLIBC__
    KnownPlatformAlign = 2 * sizeof(void*)
#else
    KnownPlatformAlign = 1
#endif
};

Q_DECL_CONSTEXPR static size_t platformMallocAlign()
{
    return qMax<size_t>(qMax<size_t>(AlignOfDouble, AlignOfPointer), KnownPlatformAlign);
}

static inline size_t calculateBlockSize(size_t &capacity, size_t objectSize, size_t headerSize,
                                        uint options)
{
    // Calculate the byte size
    // allocSize = objectSize * capacity + headerSize, but checked for overflow
    // plus padded to grow in size
    if (options & QArrayData::GrowsForward) {
        auto r = qCalculateGrowingBlockSize(capacity, objectSize, headerSize);
        capacity = r.elementCount;
        return r.size;
    } else {
        return qCalculateBlockSize(capacity, objectSize, headerSize);
    }
}

static QArrayData *allocateData(size_t allocSize, uint options)
{
    QArrayData *header = static_cast<QArrayData *>(::malloc(allocSize));
    if (header) {
        Q_ASSERT((quintptr(header) & (platformMallocAlign() - 1)) == 0);
        header->flags = options;
        header->refCounter().store(1);
    }
    return header;
}

static QArrayData *reallocateData(QArrayData *header, size_t allocSize, uint options)
{
    Q_ASSERT(!(options & QArrayData::ImmutableHeader));
    header = static_cast<QArrayData *>(::realloc(header, allocSize));
    if (header)
        header->flags = options;
    return header;
}

void *QArrayData::allocate(QArrayData **dptr, size_t objectSize, size_t alignment,
        size_t capacity, ArrayOptions options) Q_DECL_NOTHROW
{
    Q_ASSERT(dptr);
    // Alignment is a power of two
    Q_ASSERT(!(alignment & (alignment - 1)));

    if (capacity == 0) {
        // optimization for empty headers
        *dptr = const_cast<QArrayData *>(&qt_array_empty);
#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
        if (options & Unsharable)
            *data = const_cast<QArrayData *>(&qt_array_unsharable_empty);
#endif
        return sharedNullData();
    }

    size_t headerSize = sizeof(QArrayAllocatedData);
    if (alignment > platformMallocAlign()) {
        // Allocate extra (alignment - Q_ALIGNOF(QArrayAllocatedData)) padding bytes so we
        // can properly align the data array. This assumes malloc is able to
        // provide appropriate alignment for the header -- that isn't the case on x86!
        headerSize += alignment - platformMallocAlign();
    }

    if (headerSize > size_t(MaxAllocSize))
        return 0;

    size_t allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    options |= AllocatedDataType | MutableData;
    QArrayAllocatedData *header = static_cast<QArrayAllocatedData *>(allocateData(allocSize, options));
    quintptr data = 0;
    if (header) {
        // find where offset should point to so that data() is aligned to alignment bytes
        data = (quintptr(header) + sizeof(QArrayAllocatedData) + alignment - 1)
                & ~(alignment - 1);
        header->alloc = uint(capacity);
    }

    *dptr = header;
    return reinterpret_cast<void *>(data);
}

QArrayData *QArrayData::prepareRawData(ArrayOptions options) Q_DECL_NOTHROW
{
    return allocateData(sizeof(QArrayRawData), (options & ~DataTypeBits) | RawDataType);
}

QPair<QArrayData *, void *>
QArrayData::reallocateUnaligned(QArrayData *data, void *dataPointer,
                                size_t objectSize, size_t capacity, ArrayOptions options) Q_DECL_NOTHROW
{
    Q_ASSERT(data);
    Q_ASSERT(data->isMutable());
    Q_ASSERT(!data->isShared());

    options |= ArrayOption(AllocatedDataType);
    size_t headerSize = reinterpret_cast<char *>(dataPointer) - reinterpret_cast<char *>(data);
    size_t allocSize = calculateBlockSize(capacity, objectSize, headerSize, options);
    options |= AllocatedDataType | MutableData;
    QArrayAllocatedData *header = static_cast<QArrayAllocatedData *>(reallocateData(data, allocSize, options));
    if (header) {
        header->alloc = uint(capacity);
        dataPointer = reinterpret_cast<char *>(header) + headerSize;
    }
    return qMakePair(static_cast<QArrayData *>(header), dataPointer);
}

QArrayData *QArrayData::prepareForeignData(ArrayOptions options) Q_DECL_NOTHROW
{
    return allocateData(sizeof(QArrayForeignData), (options & ~DataTypeBits) | ForeignDataType);
}

void QArrayData::deallocate(QArrayData *data, size_t objectSize,
        size_t alignment) Q_DECL_NOTHROW
{
    // Alignment is a power of two
    Q_ASSERT(alignment >= Q_ALIGNOF(QArrayData)
            && !(alignment & (alignment - 1)));
    Q_UNUSED(objectSize) Q_UNUSED(alignment)

#if !defined(QT_NO_UNSHARABLE_CONTAINERS)
    if (data == &qt_array_unsharable_empty)
        return;
#endif

    if (data->flags & ForeignDataType)
        data->asForeignData()->notifyFunction(data->asForeignData()->token);

    Q_ASSERT_X(data == 0 || !data->isStatic(), "QArrayData::deallocate", "Static data can not be deleted");
    ::free(data);
}

namespace QtPrivate {
/*!
  \internal
*/
QContainerImplHelper::CutResult QContainerImplHelper::mid(int originalLength, int *_position, int *_length)
{
    int &position = *_position;
    int &length = *_length;
    if (position > originalLength)
        return Null;

    if (position < 0) {
        if (length < 0 || length + position >= originalLength)
            return Full;
        if (length + position <= 0)
            return Null;
        length += position;
        position = 0;
    } else if (uint(length) > uint(originalLength - position)) {
        length = originalLength - position;
    }

    if (position == 0 && length == originalLength)
        return Full;

    return length > 0 ? Subset : Empty;
}
}

QT_END_NAMESPACE
