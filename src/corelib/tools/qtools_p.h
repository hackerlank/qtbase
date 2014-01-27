/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef QTOOLS_P_H
#define QTOOLS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qstring.h"
#include "qsimd_p.h"
#include <limits>

QT_BEGIN_NAMESPACE

namespace QtMiscUtils {
Q_DECL_CONSTEXPR inline char toHexUpper(uint value) Q_DECL_NOTHROW
{
    return "0123456789ABCDEF"[value & 0xF];
}

Q_DECL_CONSTEXPR inline char toHexLower(uint value) Q_DECL_NOTHROW
{
    return "0123456789abcdef"[value & 0xF];
}

Q_DECL_CONSTEXPR inline int fromHex(uint c) Q_DECL_NOTHROW
{
    return ((c >= '0') && (c <= '9')) ? int(c - '0') :
           ((c >= 'A') && (c <= 'F')) ? int(c - 'A' + 10) :
           ((c >= 'a') && (c <= 'f')) ? int(c - 'a' + 10) :
           /* otherwise */              -1;
}

Q_DECL_CONSTEXPR inline char toOct(uint value) Q_DECL_NOTHROW
{
    return '0' + char(value & 0x7);
}

Q_DECL_CONSTEXPR inline int fromOct(uint c) Q_DECL_NOTHROW
{
    return ((c >= '0') && (c <= '7')) ? int(c - '0') : -1;
}
}

// We typically need an extra bit for qNextPowerOfTwo when determining the next allocation size.
enum {
    MaxAllocSize = (1 << (std::numeric_limits<int>::digits - 1)) - 1
};

// implemented in qbytearray.cpp
int Q_CORE_EXPORT qAllocMore(int alloc, int extra) Q_DECL_NOTHROW;

class QUsAsciiTransforms
{
    static uchar extract(const char *ptr) { return uchar(*ptr); }
    static ushort extract(const ushort *ptr) { return *ptr; }
    static ushort extract(const QChar *ptr) { return ptr->unicode(); }
#ifdef Q_COMPILER_UNICODE_STRINGS
    static ushort extract(const char16_t *ptr) { return *ptr; }
#endif

public:
    template <typename T>
    static int runLength(const T *begin, int len)
    {
        qintptr i = 0;
#ifdef __SSE2__
        if (sizeof(T) == 1) {
            // we're going to read data[0..15] (16 bytes)
            for ( ; i + 15 < len; i += 16) {
                __m128i chunk = _mm_loadu_si128((__m128i*)(begin + i)); // load
                // movemask extracts the high bit
                if (uint mask = _mm_movemask_epi8(chunk))
                    return int(i) + _bit_scan_forward(mask);
            }
        } else if (sizeof(T) == 2) {
            // we're going to read data[0..15] (32 bytes)
            for ( ; i + 15 < len; i += 16) {
#  ifdef __AVX2__
                const __m256i nonAscii = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(0x7f807f80));
                __m256i chunk = _mm256_loadu_si256((__m256i*)(begin + i));
                chunk = _mm256_adds_epu16(chunk, nonAscii);
                uint mask = _mm256_movemask_epi8(chunk);
#  else
                // \forall x < 0x80 -> SaturateToUnsignedWord(x + 0x7f80) < 0x8000
                // \forall x >= 0x80 -> SaturateToUnsignedWord(x + 0x7f80) >= 0x8000
                // then we use movemask to extract the high bits (remembering to ignore every other bit)
                const __m128i nonAsciiSaturate = _mm_set1_epi16(0x7f80);
                __m128i chunk1 = _mm_loadu_si128((__m128i*)(begin + i));
                __m128i chunk2 = _mm_loadu_si128((__m128i*)(begin + i + 8));
                chunk1 = _mm_adds_epu16(chunk1, nonAsciiSaturate);
                chunk2 = _mm_adds_epu16(chunk2, nonAsciiSaturate);
                uint mask = _mm_movemask_epi8(chunk1) | (_mm_movemask_epi8(chunk2) << 16);
                mask &= 0xaaaaaaaa;
#  endif
                if (mask)
                    return int(i) + _bit_scan_forward(mask);
            }
        }
#endif
        for ( ; i < len; ++i) {
            if (extract(begin + i) >= 0x80)
                break;
        }
        return int(i);
    }

    // Behavior: converts all US-ASCII words in src to dst
    // if src contains non-US-ASCII, they will be replaced by something non-US-ASCII or NUL
    template <typename T>
    static void quickToUsAscii(char *dst, const T *src, typename QtPrivate::QEnableIf<sizeof(T) == 2, int>::type len)
    {
        qptrdiff i = 0;
#ifdef __SSE2__
        // we're going to read data[0..15] (32 bytes)
        for ( ; i + 15 < len; i += 16) {
            __m128i chunk1 = _mm_loadu_si128((__m128i*)(src + i));
            __m128i chunk2 = _mm_loadu_si128((__m128i*)(src + i + 8));
            __m128i packed = _mm_packus_epi16(chunk1, chunk2);
            _mm_storeu_si128((__m128i*)(dst + i), packed);
        }
        dst += i;
        src += i;
        len &= 0xf;
        i = 0;
#endif
        for ( ; i < len; ++i) {
            ushort u = extract(src + i);
            dst[i] = u >= 0x0080 ? char(-1) : char(u);
        }
    }
};

QT_END_NAMESPACE

#endif // QTOOLS_P_H
