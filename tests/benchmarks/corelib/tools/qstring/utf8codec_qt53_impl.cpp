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

#include "private/qutfcodec_p.h"
#include "private/qsimd_p.h"

#if defined(__SSE2__)

static inline bool simdEncodeAscii(uchar *&dst, const ushort *&nextAscii, const ushort *&src, const ushort *end)
{
    // do sixteen characters at a time
    for ( ; end - src >= 16; src += 16, dst += 16) {
        __m128i data1 = _mm_loadu_si128((__m128i*)src);
        __m128i data2 = _mm_loadu_si128(1+(__m128i*)src);

        uint n1 = 0;
        ushort n = 0;

        // check if everything is ASCII
        // the highest ASCII value is U+007F
#ifdef __SSE4_2__
        // see the comment in qstring.cpp:mergeQuestionMarks for a note on _mm_cmpestrm
        // n1 will contain 2 bits set per character in [data1, data2] that is non-ASCII
        const int mode = _SIDD_UWORD_OPS | _SIDD_CMP_RANGES | _SIDD_UNIT_MASK;
        const __m128i rangeMatch = _mm_cvtsi32_si128(0xffff0080);
        __m128i r1 = _mm_cmpestrm(rangeMatch, 2, data1, 8, mode);
        __m128i r2 = _mm_cmpestrm(rangeMatch, 2, data2, 8, mode);
        n1 = _mm_movemask_epi8(r1) | _mm_movemask_epi8(r2) << 16;
#elif defined __SSE4_1__
        // n will contain 1 bit set per character in [data1, data2] that is non-ASCII
        const __m128i highest = _mm_set1_epi16(0x0080);
        data1 = _mm_min_epu16(data1, highest);
        data2 = _mm_min_epu16(data2, highest);
        n = _mm_movemask_epi8(_mm_packus_epi16(data1, data2));
#elif defined SSE2_ALT2
        // Do the packing directly:
        // The PACKUSWB instruction has packs a signed 16-bit integer to an unsigned 8-bit
        // with saturation. That is, anything from 0x0100 to 0x7fff is saturated to 0xff,
        // while all negatives (0x8000 to 0xffff) get saturated to 0x00. To detect non-ASCII,
        // we simply do a signed greater-than comparison to 0x00. That means we detect NULs as
        // "non-ASCII", but it's an acceptable compromise.
        // n will contain 1 bit set per character in [data1, data2] that is non-ASCII (or NUL)
        __m128i packed = _mm_packus_epi16(data1, data2);
        __m128i nonAscii = _mm_cmpgt_epi8(packed, _mm_setzero_si128());
        n = ~_mm_movemask_epi8(nonAscii);
#elif defined SSE2_ALT1
        // Do a double saturation: add 0xff7f (highest ASCII will be 0xfffe)
        // then subtract 0xff7f again. This saturates all non-ASCII to 0x0080.
        // n will contain 1 bit set per character in [data1, data2] that is non-ASCII
        const __m128i parcel = _mm_set1_epi16(0xff7f);
        data1 = _mm_adds_epu16(data1, parcel);
        data2 = _mm_adds_epu16(data2, parcel);
        data1 = _mm_subs_epu16(data1, parcel);
        data2 = _mm_subs_epu16(data2, parcel);
        n = _mm_movemask_epi8(_mm_packus_epi16(data1, data2));
#else
        // by doing an unsigned saturated addition of 0x7f80, the highest ASCII value
        // will result in 0x7fff, whereas all non-ASCII will be 0x8000 through 0xffff,
        // for which we can use movemask to get the highest bit
        // Note: movemask is byte-based, so we need to ignore every other bit (0x1, 0x4, 0x10, 0x40, ...)
        // Another option to avoid the bit fiddling is to either AND the incoming data with 0xff80
        // or unsigned saturate subtract 0x007f before the saturated addition of 0x7f80
        const __m128i parcel = _mm_set1_epi16(0x7f80);
        __m128i r1 = _mm_adds_epu16(data1, parcel);
        __m128i r2 = _mm_adds_epu16(data2, parcel);
        n1 = ushort(_mm_movemask_epi8(r1)) | ushort(_mm_movemask_epi8(r2)) << 16;
        n1 &= ~0x55555555;
#endif
        // pack
        _mm_storeu_si128((__m128i*)dst, _mm_packus_epi16(data1, data2));

        if (n1) {
            // at least one non-ASCII entry
            nextAscii = src + (_bit_scan_reverse(n1) + 1)/2;
            n1 = _bit_scan_forward(n1)/2;
            dst += n1;
            src += n1;
            return false;
        }
        if (n) {
            nextAscii = src + _bit_scan_reverse(n) + 1;
            n = _bit_scan_forward(n);
            dst += n;
            src += n;
            return false;
        }
    }
    nextAscii = end;
    return src == end;
}

static inline bool simdDecodeAscii(ushort *&dst, const uchar *&nextAscii, const uchar *&src, const uchar *end)
{
    // do sixteen characters at a time
    for ( ; end - src >= 16; src += 16, dst += 16) {
        __m128i data = _mm_loadu_si128((__m128i*)src);

        // check if everything is ASCII
        // movemask extracts the high bit of every byte, so n is non-zero if something isn't ASCII
        uint n = _mm_movemask_epi8(data);
        if (n) {
            while (!(n & 1)) {
                *dst++ = *src++;
                n >>= 1;
            }
            n = _bit_scan_reverse(n);
            nextAscii = src + n + 1;
            return false;
        }

        // unpack
        _mm_storeu_si128((__m128i*)dst, _mm_unpacklo_epi8(data, _mm_setzero_si128()));
        _mm_storeu_si128(1+(__m128i*)dst, _mm_unpackhi_epi8(data, _mm_setzero_si128()));
    }
    nextAscii = end;
    return src == end;
}
#else
static inline bool simdEncodeAscii(uchar *, const ushort *&nextAscii, const ushort *, const ushort *end)
{
    nextAscii = end;
    return false;
}

static inline bool simdDecodeAscii(ushort *, const uchar *&nextAscii, const uchar *, const uchar *end)
{
    nextAscii = end;
    return false;
}
#endif

int convertFromUnicode(uchar *buffer, const ushort *uc, int len)
{
    uchar *dst = buffer;
    const ushort *src = reinterpret_cast<const ushort *>(uc);
    const ushort *nextAscii;
    const ushort *const end = src + len;

    while (src != end) {
        nextAscii = 0;
        if (simdEncodeAscii(dst, nextAscii, src, end))
            break;

        do {
            ushort uc = *src++;
            int res = QUtf8Functions::toUtf8<QUtf8BaseTraits>(uc, dst, src, end);
            if (res < 0) {
                // encoding error - append '?'
                *dst++ = '?';
            }
        } while (src < nextAscii);
    }

    return dst - buffer;
}

int convertToUnicode(ushort *buffer, const char *chars, int len)
{
    ushort *dst = reinterpret_cast<ushort *>(buffer);
    const uchar *src = reinterpret_cast<const uchar *>(chars);
    const uchar *nextAscii;
    const uchar *end = src + len;

    while (src < end) {
        nextAscii = 0;
        if (simdDecodeAscii(dst, nextAscii, src, end))
            break;

        do {
            uchar b = *src++;
            int res = QUtf8Functions::fromUtf8<QUtf8BaseTraits>(b, dst, src, end);
            if (res < 0) {
                // decoding error
                *dst++ = QChar::ReplacementCharacter;
            }
        } while (src < nextAscii);
    }

    return dst - buffer;
}
