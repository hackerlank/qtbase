/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2014 Intel Corporation
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <QStringList>
#include <QFile>
#include <QtTest/QtTest>

#ifdef Q_OS_UNIX
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef QT_HAVE_ICU
#  include "unicode/ucnv.h"
#endif

// MAP_ANON is deprecated on Linux, and MAP_ANONYMOUS is not present on Mac
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

#include <private/qsimd_p.h>

#include "data.h"
Q_DECLARE_METATYPE(StringData *)

class tst_QString: public QObject
{
    Q_OBJECT
public:
    tst_QString();
private slots:
    void section_regexp_data() { section_data_impl(); }
    void section_regexp() { section_impl<QRegExp>(); }
    void section_regularexpression_data() { section_data_impl(); }
    void section_regularexpression() { section_impl<QRegularExpression>(); }
    void section_string_data() { section_data_impl(false); }
    void section_string() { section_impl<QString>(); }

    void ucstrncmp_data() const;
    void ucstrncmp() const;
    void fromUtf8() const;
    void fromLatin1_data() const;
    void fromLatin1() const;
    void fromLatin1Alternatives_data() const;
    void fromLatin1Alternatives() const;
    void toLatin1Alternatives_data() const;
    void toLatin1Alternatives() const;
    void fromUtf8Alternatives_data() const;
    void fromUtf8Alternatives() const;
    void fromUtf8Alternatives2_data() const;
    void fromUtf8Alternatives2() const;
    void toUtf8Alternatives_data() const;
    void toUtf8Alternatives() const;
    void toUtf8Alternatives2_data() const;
    void toUtf8Alternatives2() const;
    void findChar_data();
    void findChar();
    void findCharCps_data();
    void findCharCps();

    void toUpper_data();
    void toUpper();
    void toLower_data();
    void toLower();
    void toCaseFolded_data();
    void toCaseFolded();

private:
    void section_data_impl(bool includeRegExOnly = true);
    template <typename RX> void section_impl();
};

tst_QString::tst_QString()
{
}

void tst_QString::section_data_impl(bool includeRegExOnly)
{
    QTest::addColumn<QString>("s");
    QTest::addColumn<QString>("sep");
    QTest::addColumn<bool>("isRegExp");

    QTest::newRow("IPv4") << QStringLiteral("192.168.0.1") << QStringLiteral(".") << false;
    QTest::newRow("IPv6") << QStringLiteral("2001:0db8:85a3:0000:0000:8a2e:0370:7334") << QStringLiteral(":") << false;
    if (includeRegExOnly) {
        QTest::newRow("IPv6-reversed-roles") << QStringLiteral("2001:0db8:85a3:0000:0000:8a2e:0370:7334") << QStringLiteral("\\d+") << true;
        QTest::newRow("IPv6-complex") << QStringLiteral("2001:0db8:85a3:0000:0000:8a2e:0370:7334") << QStringLiteral("(\\d+):\\1") << true;
    }
}

template <typename RX>
inline QString escape(const QString &s)
{ return RX::escape(s); }

template <>
inline QString escape<QString>(const QString &s)
{ return s; }

template <typename RX>
inline void optimize(RX &) {}

template <>
inline void optimize(QRegularExpression &rx)
{ rx.optimize(); }

template <typename RX>
void tst_QString::section_impl()
{
    QFETCH(QString, s);
    QFETCH(QString, sep);
    QFETCH(bool, isRegExp);

    RX rx(isRegExp ? sep : escape<RX>(sep));
    optimize(rx);
    for (int i = 0; i < 20; ++i)
        (void) s.count(rx); // make (s, rx) hot

    QBENCHMARK {
        const QString result = s.section(rx, 0, 16);
        Q_UNUSED(result);
    }
}

static int ucstrncmp_memcmp(const ushort *a, const ushort *b, int l)
{
    return memcmp(a, b, l * 2);
}

// This function matches ucstrncmp that existed up to Qt 5.2
static int ucstrncmp_shortwise(const ushort *a, const ushort *b, int l)
{
    while (l-- && *a == *b)
        a++,b++;
    if (l==-1)
        return 0;
    return *a - *b;
}

// This function is equivalent to the qMemEquals that existed up to Qt 5.2;
// it's also the ucstrncmp used in Qt 5.3 when no special optimizations are possible
static int ucstrncmp_intwise(const ushort *a, const ushort *b, int len)
{
    // do both strings have the same alignment?
    if ((quintptr(a) & 2) == (quintptr(b) & 2)) {
        // are we aligned to 4 bytes?
        if (quintptr(a) & 2) {
            if (*a != *b)
                return *a - *b;
            ++a;
            ++b;
            --len;
        }

        const uint *p1 = (const uint *)a;
        const uint *p2 = (const uint *)b;
        quintptr counter = 0;
        for ( ; len > 1 ; len -= 2, ++counter) {
            if (p1[counter] != p2[counter]) {
                // which ushort isn't equal?
                int diff = a[2*counter] - b[2*counter];
                return diff ? diff : a[2*counter + 1] - b[2*counter + 1];
            }
        }

        return len ? a[2*counter] - b[2*counter] : 0;
    } else {
        while (len-- && *a == *b)
            a++,b++;
        if (len==-1)
            return 0;
        return *a - *b;
    }
}

#if defined(Q_CC_GNU) && defined(Q_PROCESSOR_X86)
static int ucstrncmp_repcmp(const ushort *a, const ushort *b, int len)
{
    short retval = 0;
    asm ("repz; cmpsw\n"
         "jz    0f\n"
         // reload the last word
         "mov   -2(%0), %2\n"
         "sub   -2(%1), %2\n"
         "0:"
    : "+D" (a), "+S" (b), "+r" (retval) : "c" (len));
    return retval;
}
#endif

#ifdef __SSE2__
static inline int ucstrncmp_short_tail(const ushort *p1, const ushort *p2, int len)
{
    if (len) {
        if (*p1 != *p2)
            return *p1 - *p2;
        if (--len) {
            if (p1[1] != p2[1])
                return p1[1] - p2[1];
            if (--len) {
                if (p1[2] != p2[2])
                    return p1[2] - p2[2];
                if (--len) {
                    if (p1[3] != p2[3])
                        return p1[3] - p2[3];
                    if (--len) {
                        if (p1[4] != p2[4])
                            return p1[4] - p2[4];
                        if (--len) {
                            if (p1[5] != p2[5])
                                return p1[5] - p2[5];
                            if (--len) {
                                if (p1[6] != p2[6])
                                    return p1[6] - p2[6];
                                return p1[7] - p2[7];
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

// This is the basic building block of ucstrncmp with SSE
// loop around loading twice 16 bytes;
// compare using PCMPEQW (which sets each 16-bit lane to 0 if false, -1 if true);
// then extract the highest bit from each byte using PMOVMSKB;
// then negate the result (which ensures that bits 16-31 will be set) so we can
// search for the first bit set
static int ucstrncmp_sse2(const ushort *a, const ushort *b, int l)
{
    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    a += l & ~7;
    b += l & ~7;
    l &= 7;

    // we're going to read ptr[0..15] (16 bytes)
    for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
        __m128i a_data = _mm_loadu_si128((__m128i*)ptr);
        __m128i b_data = _mm_loadu_si128((__m128i*)(ptr + distance));
        __m128i result = _mm_cmpeq_epi16(a_data, b_data);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different byte
            uint idx = uint(_bit_scan_forward(mask));
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
        }
    }
    return ucstrncmp_short_tail(reinterpret_cast<const ushort *>(ptr),
                                reinterpret_cast<const ushort *>(ptr + distance), l);
}

// This is the same as the basic building block, plus:
// after we're done with the main loop, we check if the data is 16-byte aligned;
// if it is, we can load 16 bytes again without page faults
static int ucstrncmp_sse2_alt_aligned(const ushort *a, const ushort *b, int l)
{
    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    a += l & ~7;
    b += l & ~7;
    l &= 7;

    // we're going to read ptr[0..15] (16 bytes)
    for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
        __m128i a_data = _mm_loadu_si128((__m128i*)ptr);
        __m128i b_data = _mm_loadu_si128((__m128i*)(ptr + distance));
        __m128i result = _mm_cmpeq_epi16(a_data, b_data);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different byte
            uint idx = uint(_bit_scan_forward(mask));
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
        }
    }

    if (Q_UNLIKELY(quintptr(ptr) & 0xf || distance & 0xf))
        return ucstrncmp_short_tail(reinterpret_cast<const ushort *>(ptr),
                                    reinterpret_cast<const ushort *>(ptr + distance), l);

    if (!l)
        return 0;

    // we're going to read past the end of the strings
    // from ptr[0]..ptr[l - 1], the data is valid
    // from ptr[l]..ptr[7], it's garbage
    __m128i a_data = _mm_loadu_si128((__m128i*)ptr);
    __m128i b_data = _mm_loadu_si128((__m128i*)(ptr + distance));
    __m128i result = _mm_cmpeq_epi16(a_data, b_data);
    ushort mask = ~_mm_movemask_epi8(result);
    if (mask) {
        // found a different byte
        uint idx = uint(_bit_scan_forward(mask));
        if (idx < uint(l * 2))
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
    }
    return 0;
}

// This is the same as the basic building block, plus:
// we try an alignment on the prologue on one of the two pointers,
// so we can issue aligned loads;
// this does not include the tail aligned load from the case above
static int ucstrncmp_sse2_aligning(const ushort *a, const ushort *b, int l)
{
    if (l >= 8) {
        __m128i m1 = _mm_loadu_si128((__m128i *)a);
        __m128i m2 = _mm_loadu_si128((__m128i *)b);
        __m128i cmp = _mm_cmpeq_epi16(m1, m2);
        uint mask = ~_mm_movemask_epi8(cmp);
        if (ushort(mask)) {
            // which ushort isn't equal?
            int counter = _bit_scan_forward(mask)/2;
            return a[counter] - b[counter];
        }


        // now align to do 16-byte loads
        int diff = 8 - (quintptr(a) & 0xf)/2;
        l -= diff;
        a += diff;
        b += diff;
    }

    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    a += l & ~7;
    b += l & ~7;
    l &= 7;

    // we're going to read ptr[0..15] (16 bytes)
    for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
        __m128i a_data = _mm_load_si128((__m128i*)ptr);
        __m128i b_data = _mm_loadu_si128((__m128i*)(ptr + distance));
        __m128i result = _mm_cmpeq_epi16(a_data, b_data);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different byte
            uint idx = uint(_bit_scan_forward(mask));
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
        }
    }
    return ucstrncmp_short_tail(a, b, l);
}

#ifdef __SSSE3__
// part of tests below, only used if both a and b are 16-byte aligned
static inline int ucstrncmp_sse2_aligned(const ushort *a, const ushort *b, int l)
{
    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    a += l & ~7;
    b += l & ~7;
    l &= 7;

    // we're going to read ptr[0..15] (16 bytes)
    for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
        __m128i a_data = _mm_load_si128((__m128i*)ptr);
        __m128i b_data = _mm_load_si128((__m128i*)(ptr + distance));
        __m128i result = _mm_cmpeq_epi16(a_data, b_data);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different byte
            uint idx = uint(_bit_scan_forward(mask));
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
        }
    }
    return ucstrncmp_short_tail(a, b, l);
}

// part of tests below, only used if a is aligned (but not b)
static inline int ucstrncmp_ssse3_alignr_aligned(const ushort *a, const ushort *b, int l)
{
    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    a += l & ~7;
    b += l & ~7;
    l &= 7;

    // we're going to read ptr[0..15] (16 bytes)
    for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
        __m128i a_data = _mm_load_si128((__m128i*)ptr);
        __m128i b_data = _mm_lddqu_si128((__m128i*)(ptr + distance));
        __m128i result = _mm_cmpeq_epi16(a_data, b_data);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different byte
            uint idx = uint(_bit_scan_forward(mask));
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
        }
    }
    return ucstrncmp_short_tail(a, b, l);
}

// part of tests below, only used if a is aligned (but not b)
typedef __m128i (* MMLoadFunction)(const __m128i *);
template<int N, MMLoadFunction LoadFunction>
static inline int ucstrncmp_ssse3_alignr(const ushort *a, const ushort *b, int len)
{
    qptrdiff counter = 0;
    __m128i lower, upper;
    upper = _mm_load_si128((__m128i *)a);

    do {
        lower = upper;
        upper = _mm_load_si128((__m128i *)(a + counter) + 1);
        __m128i merged = _mm_alignr_epi8(upper, lower, N);

        __m128i m2 = LoadFunction((__m128i *)(b + counter));
        __m128i cmp = _mm_cmpeq_epi16(merged, m2);
        uint mask = ~_mm_movemask_epi8(cmp);
        if (ushort(mask)) {
            // which ushort isn't equal?
            counter += _bit_scan_forward(mask)/2;
            return a[counter + N/2] - b[counter];
        }

        counter += 8;
        len -= 8;
    } while (len >= 8);

    return ucstrncmp_short_tail(a + counter + N/2, b + counter, len);
}

// external linkage to be used as the MMLoadFunction template argument for ucstrncmp_ssse3_alignr
__m128i EXT_mm_lddqu_si128(const __m128i *p)
{ return _mm_lddqu_si128(p); }
__m128i EXT_mm_load_si128(__m128i const *p)
{ return _mm_load_si128(p); }

// This is the same as the basic building block, plus:
// always loads aligned from a and uses PALIGNR to ensure that the data from a and b line up
static int ucstrncmp_ssse3(const ushort *a, const ushort *b, int len)
{
    if (len >= 8) {
        int val = quintptr(a) & 0xf;
        a -= val/2;

        if (Q_LIKELY(val == 0))
            return ucstrncmp_ssse3_alignr_aligned(a, b, len);
        if (Q_LIKELY(val == 8))
            return ucstrncmp_ssse3_alignr<8, EXT_mm_lddqu_si128>(a, b, len);

        // all the rest are almost equally unlikely
        if (val < 8) {
            if (val < 4)
                return ucstrncmp_ssse3_alignr<2, EXT_mm_lddqu_si128>(a, b, len);
            else if (val == 4)
                    return ucstrncmp_ssse3_alignr<4, EXT_mm_lddqu_si128>(a, b, len);
            else
                    return ucstrncmp_ssse3_alignr<6, EXT_mm_lddqu_si128>(a, b, len);
        } else {
            if (val < 12)
                return ucstrncmp_ssse3_alignr<10, EXT_mm_lddqu_si128>(a, b, len);
            else if (val == 12)
                return ucstrncmp_ssse3_alignr<12, EXT_mm_lddqu_si128>(a, b, len);
            else
                return ucstrncmp_ssse3_alignr<14, EXT_mm_lddqu_si128>(a, b, len);
        }
    }
    return ucstrncmp_short_tail(a, b, len);
}

// This is the same as the basic building block, plus:
// first, it does a prologue alignment on b (same as ucstrncmp_sse2_aligning);
// then it uses PALIGNR to do aligned loads on a;
// this way, all loads are aligned
static int ucstrncmp_ssse3_aligning(const ushort *a, const ushort *b, int len)
{
    if (len >= 8) {
        __m128i m1 = _mm_loadu_si128((__m128i *)a);
        __m128i m2 = _mm_loadu_si128((__m128i *)b);
        __m128i cmp = _mm_cmpeq_epi16(m1, m2);
        uint mask = ~_mm_movemask_epi8(cmp);
        if (ushort(mask)) {
            // which ushort isn't equal?
            int counter = _bit_scan_forward(mask)/2;
            return a[counter] - b[counter];
        }


        // now 'b' align to do 16-byte loads
        int diff = 8 - (quintptr(b) & 0xf)/2;
        len -= diff;
        a += diff;
        b += diff;
    }

    if (len < 8)
        return ucstrncmp_short_tail(a, b, len);

    // 'b' is aligned
    int val = quintptr(a) & 0xf;
    a -= val/2;

    if (Q_LIKELY(val == 0))
        return ucstrncmp_sse2_aligned(a, b, len);
    else if (Q_LIKELY(val == 8))
        return ucstrncmp_ssse3_alignr<8, EXT_mm_load_si128>(a, b, len);
    if (val < 8) {
        if (val < 4)
            return ucstrncmp_ssse3_alignr<2, EXT_mm_load_si128>(a, b, len);
        else if (val == 4)
            return ucstrncmp_ssse3_alignr<4, EXT_mm_load_si128>(a, b, len);
        else
            return ucstrncmp_ssse3_alignr<6, EXT_mm_load_si128>(a, b, len);
    } else {
        if (val < 12)
            return ucstrncmp_ssse3_alignr<10, EXT_mm_load_si128>(a, b, len);
        else if (val == 12)
            return ucstrncmp_ssse3_alignr<12, EXT_mm_load_si128>(a, b, len);
        else
            return ucstrncmp_ssse3_alignr<14, EXT_mm_load_si128>(a, b, len);
    }
}

static inline
int ucstrncmp_ssse3_aligning2_aligned(const ushort *a, const ushort *b, int len, int garbage)
{
    // len >= 8
    __m128i m1 = _mm_load_si128((const __m128i *)a);
    __m128i m2 = _mm_load_si128((const __m128i *)b);
    __m128i cmp = _mm_cmpeq_epi16(m1, m2);
    int mask = short(_mm_movemask_epi8(cmp)); // force sign extension
    mask >>= garbage;
    if (~mask) {
        // which ushort isn't equal?
        uint counter = (garbage + _bit_scan_forward(~mask));
        return a[counter/2] - b[counter/2];
    }

    // the first 16-garbage bytes (8-garbage/2 ushorts) were equal
    len -= 8 - garbage/2;
    return ucstrncmp_sse2_aligned(a + 8, b + 8, len);
}

template<int N> static inline
int ucstrncmp_ssse3_aligning2_alignr(const ushort *a, const ushort *b, int len, int garbage)
{
    // len >= 8
    __m128i lower, upper, merged;
    lower = _mm_load_si128((const __m128i*)a);
    upper = _mm_load_si128((const __m128i*)(a + 8));
    merged = _mm_alignr_epi8(upper, lower, N);

    __m128i m2 = _mm_load_si128((const __m128i*)b);
    __m128i cmp = _mm_cmpeq_epi16(merged, m2);
    int mask = short(_mm_movemask_epi8(cmp)); // force sign extension
    mask >>= garbage;
    if (~mask) {
        // which ushort isn't equal?
        uint counter = (garbage + _bit_scan_forward(~mask));
        return a[counter/2 + N/2] - b[counter/2];
    }

    // the first 16-garbage bytes (8-garbage/2 ushorts) were equal
    quintptr counter = 8;
    len -= 8 - garbage/2;
    while (len >= 8) {
        lower = upper;
        upper = _mm_load_si128((__m128i *)(a + counter) + 1);
        merged = _mm_alignr_epi8(upper, lower, N);

        m2 = _mm_load_si128((__m128i *)(b + counter));
        cmp = _mm_cmpeq_epi16(merged, m2);
        uint mask = ~_mm_movemask_epi8(cmp);
        if (ushort(mask)) {
            // which ushort isn't equal?
            counter += _bit_scan_forward(mask)/2;
            return a[counter + N/2] - b[counter];
        }

        counter += 8;
        len -= 8;
    }

    return ucstrncmp_short_tail(a + counter + N/2, b + counter, len);
}

static int ucstrncmp_ssse3_aligning2(const ushort *a, const ushort *b, int len)
{
    // Different strategy from above: instead of doing two unaligned loads
    // when trying to align, we'll only do aligned loads and round down the
    // addresses of a and b. This means the first load will contain garbage
    // in the beginning of the string, which we'll shift out of the way
    // (after _mm_movemask_epi8)

    if (len < 8)
        return ucstrncmp_intwise(a, b, len);

    // both a and b are misaligned
    // we'll call the alignr function with the alignment *difference* between the two
    int offset = (quintptr(a) & 0xf) - (quintptr(b) & 0xf);
    if (offset >= 0) {
        // from this point on, b has the shortest alignment
        // and align(a) = align(b) + offset
        // round down the alignment so align(b) == align(a) == 0
        int garbage = (quintptr(b) & 0xf);
        a = (const ushort*)(quintptr(a) & ~0xf);
        b = (const ushort*)(quintptr(b) & ~0xf);

        // now the first load of b will load 'garbage' extra bytes
        // and the first load of a will load 'garbage + offset' extra bytes
        if (offset == 0)
            return ucstrncmp_ssse3_aligning2_aligned(a, b, len, garbage);
        if (offset == 8)
            return ucstrncmp_ssse3_aligning2_alignr<8>(a, b, len, garbage);
        if (offset < 8) {
            if (offset < 4)
                return ucstrncmp_ssse3_aligning2_alignr<2>(a, b, len, garbage);
            else if (offset == 4)
                return ucstrncmp_ssse3_aligning2_alignr<4>(a, b, len, garbage);
            else
                return ucstrncmp_ssse3_aligning2_alignr<6>(a, b, len, garbage);
        } else {
            if (offset < 12)
                return ucstrncmp_ssse3_aligning2_alignr<10>(a, b, len, garbage);
            else if (offset == 12)
                return ucstrncmp_ssse3_aligning2_alignr<12>(a, b, len, garbage);
            else
                return ucstrncmp_ssse3_aligning2_alignr<14>(a, b, len, garbage);
        }
    } else {
        // same as above but inverted
        int garbage = (quintptr(a) & 0xf);
        a = (const ushort*)(quintptr(a) & ~0xf);
        b = (const ushort*)(quintptr(b) & ~0xf);

        offset = -offset;
        if (offset == 8)
            return -ucstrncmp_ssse3_aligning2_alignr<8>(b, a, len, garbage);
        if (offset < 8) {
            if (offset < 4)
                return -ucstrncmp_ssse3_aligning2_alignr<2>(b, a, len, garbage);
            else if (offset == 4)
                return -ucstrncmp_ssse3_aligning2_alignr<4>(b, a, len, garbage);
            else
                return -ucstrncmp_ssse3_aligning2_alignr<6>(b, a, len, garbage);
        } else {
            if (offset < 12)
                return -ucstrncmp_ssse3_aligning2_alignr<10>(b, a, len, garbage);
            else if (offset == 12)
                return -ucstrncmp_ssse3_aligning2_alignr<12>(b, a, len, garbage);
            else
                return -ucstrncmp_ssse3_aligning2_alignr<14>(b, a, len, garbage);
        }
    }
}

#ifdef __SSE4_2__
// This is not based on the basic building block:
// instead, it tries to use the SSE4.2 string compare instruction
static int ucstrncmp_pcmpestrm(const ushort *a, const ushort *b, int l)
{
    // We use the pcmpestrm instruction searching for differences (negative polarity)
    // it will reset CF if it's all equal
    // it will reset OF if the first char is equal
    // it will set ZF & SF if the length is less than 8 (which means we've done the last operation)
    // the three possible conditions are:
    //  difference found:         CF = 1
    //  all equal, not finished:  CF = ZF = SF = 0
    //  all equal, finished:      CF = 0, ZF = SF = 1
    enum { Mode = _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT };

    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    if (l >= 8) {
        a += l & ~7;
        b += l & ~7;
        l &= 7;

        for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
            // we're going to read ptr[0..15] (16 bytes)
            __m128i a_data = _mm_loadu_si128((__m128i*)ptr);
            __m128i b_data = _mm_loadu_si128((__m128i*)(ptr + distance));

            bool cmp_c = _mm_cmpestrc(a_data, 8, b_data, 8, Mode);
            if (!cmp_c)
                continue; // all equal

            uint idx = _mm_cmpestri(a_data, 8, b_data, 8, Mode);
            // found a different byte
            return reinterpret_cast<const QChar *>(ptr)[idx].unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance)[idx].unicode();
        }
    }
    return ucstrncmp_short_tail(reinterpret_cast<const ushort *>(ptr),
                                reinterpret_cast<const ushort *>(ptr + distance), l);
}

// Same as above, except that it verifies if a and b are aligned;
// if they are, we can use PCMPESTRI for one extra loop
static int ucstrncmp_pcmpestrm_alt_aligned(const ushort *a, const ushort *b, int l)
{
    // See above for more information
    enum { Mode = _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT };

    if (Q_UNLIKELY(quintptr(a) & 0xf || quintptr(b) & 0xf))
        return ucstrncmp_pcmpestrm(a, b, l);

    // we're operating on aligned data here, which allows us to load past the end
    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    l += 8;
#ifdef Q_CC_GNU
    int result = l;
    qregisterint tmp;
    asm ("0:\n"
         " add      $16, %[ptr]\n"
         " sub      $8, %%eax\n"
         " mov      %%eax, %%edx\n"

#  ifdef __AVX__
         " vmovdqa  -0x10(%[ptr]), %%xmm0\n"
         " vpcmpestri %[Mode], -0x10(%[ptr],%[distance]), %%xmm0\n"
#  else
         " movdqa   -0x10(%[ptr]), %%xmm0\n"
         " pcmpestri %[Mode], -0x10(%[ptr],%[distance]), %%xmm0\n"
#  endif
         " ja       0b\n"

         // found a difference or finished
         " mov      $0, %%eax\n"
         " jnc      1f\n"
         " add      %[ptr], %[distance]\n"
         " movzwl   -0x10(%[ptr], %[tmp], 2), %%eax\n"
         " movzwl   -0x10(%[distance], %[tmp], 2), %%ecx\n"
         " sub      %%ecx, %%eax\n"
         "1:"
         : "+a" (result),
           [tmp] "=&c" (tmp)
         : [ptr] "r" (ptr),
           [distance] "r" (distance),
           [Mode] "i" (Mode)
         : "%edx", "%xmm0");
    return result;
#else
    forever {
        // we're going to read ptr[0..15] (16 bytes)
        ptr += 16;
        l -= 8;
        __m128i a_data = _mm_load_si128((__m128i*)(ptr - 16));
        __m128i b_data = _mm_load_si128((__m128i*)(ptr - 16 + distance));

        bool cmp_a = _mm_cmpestra(a_data, l, b_data, l, Mode);
        if (cmp_a)
            continue; // CF == 0 && ZF == 0 -> all equal and not finished

        bool cmp_c = _mm_cmpestrc(a_data, l, b_data, l, Mode);
        if (cmp_c) {
            uint idx = _mm_cmpestri(a_data, l, b_data, l, Mode);
            // found a different byte
            return reinterpret_cast<const QChar *>(ptr - 16)[idx].unicode()
                    - reinterpret_cast<const QChar *>(ptr - 16 + distance)[idx].unicode();
        }

        return 0;
    }
#endif
}
#endif // __SSE4_2__
#endif // __SSSE3__
#endif // __SSE2__

typedef int (* UcstrncmpFunction)(const ushort *, const ushort *, int);
Q_DECLARE_METATYPE(UcstrncmpFunction)
extern StringData ucstrncmpData;

void tst_QString::ucstrncmp_data() const
{
    QTest::addColumn<UcstrncmpFunction>("function");
    QTest::newRow("selftest") << UcstrncmpFunction(0);
    QTest::newRow("memcmp") << &ucstrncmp_memcmp;
    QTest::newRow("shortwise") << &ucstrncmp_shortwise;
    QTest::newRow("intwise") << &ucstrncmp_intwise;
#if defined(Q_CC_GNU) && defined(Q_PROCESSOR_X86)
    QTest::newRow("repcmp") << &ucstrncmp_repcmp;
#endif
#ifdef __SSE2__
    QTest::newRow("sse2") << &ucstrncmp_sse2;
    QTest::newRow("sse2_aligned") << &ucstrncmp_sse2_alt_aligned;
    QTest::newRow("sse2_aligning") << &ucstrncmp_sse2_aligning;
#ifdef __SSSE3__
    QTest::newRow("ssse3") << &ucstrncmp_ssse3;
    QTest::newRow("ssse3_aligning") << &ucstrncmp_ssse3_aligning;
    QTest::newRow("ssse3_aligning2") << &ucstrncmp_ssse3_aligning2;
#ifdef __SSE4_2__
    QTest::newRow("sse4.2") << &ucstrncmp_pcmpestrm;
    QTest::newRow("sse4.2_aligned") << &ucstrncmp_pcmpestrm_alt_aligned;
#endif
#endif
#endif
}

void tst_QString::ucstrncmp() const
{
    const StringData::Entry *entries = reinterpret_cast<const StringData::Entry *>(ucstrncmpData.entries);
    QFETCH(UcstrncmpFunction, function);
    if (!function) {
        static const UcstrncmpFunction func[] = {
            &ucstrncmp_shortwise,
            &ucstrncmp_intwise,
#if defined(Q_CC_GNU) && defined(Q_PROCESSOR_X86)
            &ucstrncmp_repcmp,
#endif
#ifdef __SSE2__
            &ucstrncmp_sse2,
            &ucstrncmp_sse2_alt_aligned,
            &ucstrncmp_sse2_aligning,
#ifdef __SSSE3__
            &ucstrncmp_ssse3,
            &ucstrncmp_ssse3_aligning,
            &ucstrncmp_ssse3_aligning2,
#ifdef __SSE4_2__
            &ucstrncmp_pcmpestrm,
            &ucstrncmp_pcmpestrm_alt_aligned
#endif
#endif
#endif
        };
        static const int functionCount = sizeof func / sizeof func[0];

#ifdef Q_OS_UNIX
        const long pagesize = sysconf(_SC_PAGESIZE);
        void *page1, *page3;
        ushort *page2;
        page1 = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        page2 = (ushort *)mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        page3 = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        Q_ASSERT(quintptr(page2) == quintptr(page1) + pagesize || quintptr(page2) == quintptr(page1) - pagesize);
        Q_ASSERT(quintptr(page3) == quintptr(page2) + pagesize || quintptr(page3) == quintptr(page2) - pagesize);
        munmap(page1, pagesize);
        munmap(page3, pagesize);

        // populate our page
        for (uint i = 0; i < pagesize / sizeof(long long); ++i)
            ((long long *)page2)[i] = Q_INT64_C(0x0041004100410041);

        // the following should crash:
        //page2[-1] = 0xdead;
        //page2[pagesize / sizeof(ushort) + 1] = 0xbeef;

        static const ushort needle[] = {
            0x41, 0x41, 0x41, 0x41,   0x41, 0x41, 0x41, 0x41,
            0x41, 0x41, 0x41, 0x41,   0x41, 0x41, 0x41, 0x41,
            0x41
        };

        for (int algo = 0; algo < functionCount; ++algo) {
            // boundary condition test:
            for (int i = 0; i < 8; ++i) {
                (func[algo])(page2 + i, needle, sizeof needle / 2);
                (func[algo])(page2 - i - 1 - sizeof(needle)/2 + pagesize/2, needle, sizeof needle/2);
            }
        }

        munmap(page2, pagesize);
#endif

        for (int algo = 0; algo < functionCount; ++algo) {
            for (int i = 0; i < ucstrncmpData.entryCount; ++i) {
                const ushort *p1 = ucstrncmpData.ushortData + entries[i].offset1/2;
                const ushort *p2 = ucstrncmpData.ushortData + entries[i].offset2/2;
                int expected = ucstrncmp_shortwise(p1, p2, entries[i].len1/2);
                expected = qBound(-1, expected, 1);

                int result = (func[algo])(p1, p2, entries[i].len1/2);
                result = qBound(-1, result, 1);
                if (expected != result)
                    qWarning().nospace()
                        << "algo=" << algo
                        << " i=" << i
                        << " failed (" << result << "!=" << expected
                        << "); strings were "
                        << QString::fromUtf16(p1, entries[i].len1/2)
                        << " and "
                        << QString::fromUtf16(p2, entries[i].len1/2);
            }
        }
        return;
    }

    QBENCHMARK {
        for (int i = 0; i < ucstrncmpData.entryCount; ++i) {
            const ushort *p1 = ucstrncmpData.ushortData + entries[i].offset1/2;
            const ushort *p2 = ucstrncmpData.ushortData + entries[i].offset2/2;
            (function)(p1, p2, entries[i].len1/2);
        }
    }
}

void tst_QString::fromUtf8() const
{
    QString testFile = QFINDTESTDATA("utf-8.txt");
    QVERIFY2(!testFile.isEmpty(), "cannot find test file utf-8.txt!");
    QFile file(testFile);
    if (!file.open(QFile::ReadOnly)) {
        qFatal("Cannot open input file");
        return;
    }
    QByteArray data = file.readAll();
    const char *d = data.constData();
    int size = data.size();

    QBENCHMARK {
        QString::fromUtf8(d, size);
    }
}

void tst_QString::fromLatin1_data() const
{
    QTest::addColumn<QByteArray>("latin1");

    // make all the strings have the same length
    QTest::newRow("ascii-only") << QByteArray("HelloWorld");
    QTest::newRow("ascii+control") << QByteArray("Hello\1\r\n\x7f\t");
    QTest::newRow("ascii+nul") << QByteArray("a\0zbc\0defg", 10);
    QTest::newRow("non-ascii") << QByteArray("\x80\xc0\xff\x81\xc1\xfe\x90\xd0\xef\xa0");
}

void tst_QString::fromLatin1() const
{
    QFETCH(QByteArray, latin1);

    while (latin1.length() < 128) {
        latin1 += latin1;
    }

    QByteArray copy1 = latin1, copy2 = latin1, copy3 = latin1;
    copy1.chop(1);
    copy2.detach();
    copy3 += latin1; // longer length
    copy2.clear();

    QBENCHMARK {
        QString s1 = QString::fromLatin1(latin1);
        QString s2 = QString::fromLatin1(latin1);
        QString s3 = QString::fromLatin1(copy1);
        QString s4 = QString::fromLatin1(copy3);
        s3 = QString::fromLatin1(copy3);
    }
}

typedef void (* FromLatin1Function)(ushort *, const char *, int);
Q_DECLARE_METATYPE(FromLatin1Function)

void fromLatin1_regular(ushort *dst, const char *str, int size)
{
    // from qstring.cpp:
    while (size--)
        *dst++ = (uchar)*str++;
}

#ifdef __SSE2__
void fromLatin1_sse2_qt47(ushort *dst, const char *str, int size)
{
    if (size >= 16) {
        int chunkCount = size >> 4; // divided by 16
        const __m128i nullMask = _mm_set1_epi32(0);
        for (int i = 0; i < chunkCount; ++i) {
            const __m128i chunk = _mm_loadu_si128((__m128i*)str); // load
            str += 16;

            // unpack the first 8 bytes, padding with zeros
            const __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullMask);
            _mm_storeu_si128((__m128i*)dst, firstHalf); // store
            dst += 8;

            // unpack the last 8 bytes, padding with zeros
            const __m128i secondHalf = _mm_unpackhi_epi8 (chunk, nullMask);
            _mm_storeu_si128((__m128i*)dst, secondHalf); // store
            dst += 8;
        }
        size = size % 16;
    }
    while (size--)
        *dst++ = (uchar)*str++;
}

static inline void fromLatin1_epilog(ushort *dst, const char *str, int size)
{
    if (!size) return;
    dst[0] = (uchar)str[0];
    if (!--size) return;
    dst[1] = (uchar)str[1];
    if (!--size) return;
    dst[2] = (uchar)str[2];
    if (!--size) return;
    dst[3] = (uchar)str[3];
    if (!--size) return;
    dst[4] = (uchar)str[4];
    if (!--size) return;
    dst[5] = (uchar)str[5];
    if (!--size) return;
    dst[6] = (uchar)str[6];
    if (!--size) return;
    dst[7] = (uchar)str[7];
    if (!--size) return;
    dst[8] = (uchar)str[8];
    if (!--size) return;
    dst[9] = (uchar)str[9];
    if (!--size) return;
    dst[10] = (uchar)str[10];
    if (!--size) return;
    dst[11] = (uchar)str[11];
    if (!--size) return;
    dst[12] = (uchar)str[12];
    if (!--size) return;
    dst[13] = (uchar)str[13];
    if (!--size) return;
    dst[14] = (uchar)str[14];
    if (!--size) return;
    dst[15] = (uchar)str[15];
}

void fromLatin1_sse2_improved(ushort *dst, const char *str, int size)
{
    const __m128i nullMask = _mm_set1_epi32(0);
    qptrdiff counter = 0;
    size -= 16;
    while (size >= counter) {
        const __m128i chunk = _mm_loadu_si128((__m128i*)(str + counter)); // load

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter), firstHalf); // store

        // unpack the last 8 bytes, padding with zeros
        const __m128i secondHalf = _mm_unpackhi_epi8 (chunk, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter + 8), secondHalf); // store

        counter += 16;
    }
    size += 16;
    fromLatin1_epilog(dst + counter, str + counter, size - counter);
}

void fromLatin1_sse2_improved2(ushort *dst, const char *str, int size)
{
    const __m128i nullMask = _mm_set1_epi32(0);
    qptrdiff counter = 0;
    size -= 32;
    while (size >= counter) {
        const __m128i chunk1 = _mm_loadu_si128((__m128i*)(str + counter)); // load
        const __m128i chunk2 = _mm_loadu_si128((__m128i*)(str + counter + 16)); // load

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf1 = _mm_unpacklo_epi8(chunk1, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter), firstHalf1); // store

        // unpack the last 8 bytes, padding with zeros
        const __m128i secondHalf1 = _mm_unpackhi_epi8(chunk1, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter + 8), secondHalf1); // store

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf2 = _mm_unpacklo_epi8(chunk2, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter + 16), firstHalf2); // store

        // unpack the last 8 bytes, padding with zeros
        const __m128i secondHalf2 = _mm_unpackhi_epi8(chunk2, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter + 24), secondHalf2); // store

        counter += 32;
    }
    size += 16;
    if (size >= counter) {
        const __m128i chunk = _mm_loadu_si128((__m128i*)(str + counter)); // load

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter), firstHalf); // store

        // unpack the last 8 bytes, padding with zeros
        const __m128i secondHalf = _mm_unpackhi_epi8 (chunk, nullMask);
        _mm_storeu_si128((__m128i*)(dst + counter + 8), secondHalf); // store

        counter += 16;
    }
    size += 16;
    fromLatin1_epilog(dst + counter, str + counter, size - counter);
}

void fromLatin1_prolog_unrolled(ushort *dst, const char *str, int size)
{
    // QString's data pointer is most often ending in 0x2 or 0xa
    // that means the two most common values for size are (8-1)=7 and (8-5)=3
    if (size == 7)
        goto copy_7;
    if (size == 3)
        goto copy_3;

    if (size == 6)
        goto copy_6;
    if (size == 5)
        goto copy_5;
    if (size == 4)
        goto copy_4;
    if (size == 2)
        goto copy_2;
    if (size == 1)
        goto copy_1;
    return;

copy_7:
    dst[6] = (uchar)str[6];
copy_6:
    dst[5] = (uchar)str[5];
copy_5:
    dst[4] = (uchar)str[4];
copy_4:
    dst[3] = (uchar)str[3];
copy_3:
    dst[2] = (uchar)str[2];
copy_2:
    dst[1] = (uchar)str[1];
copy_1:
    dst[0] = (uchar)str[0];
}

void fromLatin1_prolog_sse2_overcommit(ushort *dst, const char *str, int)
{
    // do one iteration of conversion
    const __m128i chunk = _mm_loadu_si128((__m128i*)str); // load

    // unpack only the first 8 bytes, padding with zeros
    const __m128i nullMask = _mm_set1_epi32(0);
    const __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullMask);
    _mm_storeu_si128((__m128i*)dst, firstHalf); // store
}

template<FromLatin1Function prologFunction>
void fromLatin1_sse2_withprolog(ushort *dst, const char *str, int size)
{
    // same as the improved code, but we attempt to align at the prolog
    // therefore, we issue aligned stores

    if (size >= 16) {
        uint misalignment = uint(quintptr(dst) & 0xf);
        uint prologCount = (16 - misalignment) / 2;

        prologFunction(dst, str, prologCount);

        size -= prologCount;
        dst += prologCount;
        str += prologCount;
    }

    const __m128i nullMask = _mm_set1_epi32(0);
    qptrdiff counter = 0;
    size -= 16;
    while (size >= counter) {
        const __m128i chunk = _mm_loadu_si128((__m128i*)(str + counter)); // load

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullMask);
        _mm_store_si128((__m128i*)(dst + counter), firstHalf); // store

        // unpack the last 8 bytes, padding with zeros
        const __m128i secondHalf = _mm_unpackhi_epi8 (chunk, nullMask);
        _mm_store_si128((__m128i*)(dst + counter + 8), secondHalf); // store

        counter += 16;
    }
    size += 16;
    fromLatin1_epilog(dst + counter, str + counter, size - counter);
}

#ifdef __SSE4_1__
void fromLatin1_sse4_pmovzxbw(ushort *dst, const char *str, int size)
{
    qptrdiff counter = 0;
    size -= 16;
    while (size >= counter) {
        __m128i chunk = _mm_loadu_si128((__m128i*)(str + counter)); // load

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf = _mm_cvtepu8_epi16(chunk);
        _mm_storeu_si128((__m128i*)(dst + counter), firstHalf); // store

        // unpack the last 8 bytes, padding with zeros
        chunk = _mm_srli_si128(chunk, 8);
        const __m128i secondHalf = _mm_cvtepu8_epi16(chunk);
        _mm_storeu_si128((__m128i*)(dst + counter + 8), secondHalf); // store

        counter += 16;
    }
    size += 16;
    fromLatin1_epilog(dst + counter, str + counter, size - counter);
}

void fromLatin1_prolog_sse4_overcommit(ushort *dst, const char *str, int)
{
    // load 8 bytes and zero-extend them to 16
    const __m128i chunk = _mm_cvtepu8_epi16(*(__m128i*)str); // load
    _mm_storeu_si128((__m128i*)dst, chunk); // store
}

#ifdef __AVX2__
void fromLatin1_avx2(ushort *dst, const char *str, int size)
{
    qptrdiff counter = 0;
    size -= 16;
    while (size >= counter) {
        const __m128i chunk = _mm_loadu_si128((__m128i*)(str + counter)); // load

        // zero extend to an YMM register
        const __m256i extended = _mm256_cvtepu8_epi16(chunk);

        // store
        _mm256_storeu_si256((__m256i*)(dst + counter), extended);

        counter += 16;
    }
    size += 16;
    fromLatin1_epilog(dst + counter, str + counter, size - counter);
}

#endif // __AVX2__
#endif
#endif

#ifdef __ARM_NEON__
static inline void fromLatin1_epilog(ushort *dst, const char *str, int size)
{
    if (!size) return;
    dst[0] = (uchar)str[0];
    if (!--size) return;
    dst[1] = (uchar)str[1];
    if (!--size) return;
    dst[2] = (uchar)str[2];
    if (!--size) return;
    dst[3] = (uchar)str[3];
    if (!--size) return;
    dst[4] = (uchar)str[4];
    if (!--size) return;
    dst[5] = (uchar)str[5];
    if (!--size) return;
    dst[6] = (uchar)str[6];
    if (!--size) return;
    dst[7] = (uchar)str[7];
    if (!--size) return;
}

void fromLatin1_neon_improved(ushort *dst, const char *str, int len)
{
    while (len >= 8) {
        // load 8 bytes into one doubleword Neon register
        const uint8x8_t chunk = vld1_u8((uint8_t *)str);
        str += 8;

        // expand 8 bytes into 16 bytes in a quadword register
        const uint16x8_t expanded = vmovl_u8(chunk);
        vst1q_u16(dst, expanded); // store
        dst += 8;

        len -= 8;
    }
    fromLatin1_epilog(dst, str, len);
}

void fromLatin1_neon_improved2(ushort *dst, const char *str, int len)
{
    while (len >= 16) {
        // load 16 bytes into one quadword Neon register
        const uint8x16_t chunk = vld1q_u8((uint8_t *)str);
        str += 16;

        // expand each doubleword of the quadword register into a quadword
        const uint16x8_t expanded_low = vmovl_u8(vget_low_u8(chunk));
        vst1q_u16(dst, expanded_low); // store
        dst += 8;
        const uint16x8_t expanded_high = vmovl_u8(vget_high_u8(chunk));
        vst1q_u16(dst, expanded_high); // store
        dst += 8;

        len -= 16;
    }

    if (len >= 8) {
        // load 8 bytes into one doubleword Neon register
        const uint8x8_t chunk = vld1_u8((uint8_t *)str);
        str += 8;

        // expand 8 bytes into 16 bytes in a quadword register
        const uint16x8_t expanded = vmovl_u8(chunk);
        vst1q_u16(dst, expanded); // store
        dst += 8;

        len -= 8;
    }
    fromLatin1_epilog(dst, str, len);
}

void fromLatin1_neon_handwritten(ushort *dst, const char *str, int len)
{
    // same as above, but handwritten Neon
    while (len >= 8) {
        uint16x8_t chunk;
        asm (
            "vld1.8     %[chunk], [%[str]]!\n"
            "vmovl.u8   %q[chunk], %[chunk]\n"
            "vst1.16    %h[chunk], [%[dst]]!\n"
            : [dst] "+r" (dst),
              [str] "+r" (str),
              [chunk] "=w" (chunk));
        len -= 8;
    }

    fromLatin1_epilog(dst, str, len);
}

void fromLatin1_neon_handwritten2(ushort *dst, const char *str, int len)
{
    // same as above, but handwritten Neon
    while (len >= 16) {
        uint16x8_t chunk1, chunk2;
        asm (
            "vld1.8     %h[chunk1], [%[str]]!\n"
            "vmovl.u8   %q[chunk2], %f[chunk1]\n"
            "vmovl.u8   %q[chunk1], %e[chunk1]\n"
            "vst1.16    %h[chunk1], [%[dst]]!\n"
            "vst1.16    %h[chunk2], [%[dst]]!\n"
          : [dst] "+r" (dst),
            [str] "+r" (str),
            [chunk1] "=w" (chunk1),
            [chunk2] "=w" (chunk2));
        len -= 16;
    }

    if (len >= 8) {
        uint16x8_t chunk;
        asm (
            "vld1.8     %[chunk], [%[str]]!\n"
            "vmovl.u8   %q[chunk], %[chunk]\n"
            "vst1.16    %h[chunk], [%[dst]]!\n"
            : [dst] "+r" (dst),
              [str] "+r" (str),
              [chunk] "=w" (chunk));
        len -= 8;
    }

    fromLatin1_epilog(dst, str, len);
}
#endif

void tst_QString::fromLatin1Alternatives_data() const
{
    QTest::addColumn<FromLatin1Function>("function");
    QTest::newRow("empty") << FromLatin1Function(0);
    QTest::newRow("regular") << &fromLatin1_regular;
#ifdef __SSE2__
    QTest::newRow("sse2-qt4.7") << &fromLatin1_sse2_qt47;
    QTest::newRow("sse2-improved") << &fromLatin1_sse2_improved;
    QTest::newRow("sse2-improved2") << &fromLatin1_sse2_improved2;
    QTest::newRow("sse2-with-prolog-regular") << &fromLatin1_sse2_withprolog<&fromLatin1_regular>;
    QTest::newRow("sse2-with-prolog-unrolled") << &fromLatin1_sse2_withprolog<&fromLatin1_prolog_unrolled>;
    QTest::newRow("sse2-with-prolog-sse2-overcommit") << &fromLatin1_sse2_withprolog<&fromLatin1_prolog_sse2_overcommit>;
#ifdef __SSE4_1__
    QTest::newRow("sse2-with-prolog-sse4-overcommit") << &fromLatin1_sse2_withprolog<&fromLatin1_prolog_sse4_overcommit>;
    QTest::newRow("sse4-pmovzxbw") << &fromLatin1_sse4_pmovzxbw;
#ifdef __AVX2__
    QTest::newRow("avx2") << &fromLatin1_avx2;
#endif
#endif
#endif
#ifdef __ARM_NEON__
    QTest::newRow("neon-improved") << &fromLatin1_neon_improved;
    QTest::newRow("neon-improved2") << &fromLatin1_neon_improved2;
    QTest::newRow("neon-handwritten") << &fromLatin1_neon_handwritten;
    QTest::newRow("neon-handwritten2") << &fromLatin1_neon_handwritten2;
#endif
}

extern StringData fromLatin1Data;
static void fromLatin1Alternatives_internal(FromLatin1Function function, QString &dst, bool doVerify)
{
    const StringData::Entry *entries = fromLatin1Data.entries;

    for (int i = 0; i < fromLatin1Data.entryCount; ++i) {
        int len = entries[i].len1;
        const char *src = fromLatin1Data.charData + entries[i].offset1;

        if (!function)
            continue;
        if (!doVerify) {
            (function)(&dst.data()->unicode(), src, len);
        } else {
            dst.fill(QChar('x'), dst.length());

            (function)(&dst.data()->unicode() + 8, src, len);

            QString zeroes(8, QChar('x'));
            QString final = dst.mid(8, len);
            QCOMPARE(final, QString::fromLatin1(src, len));
            QCOMPARE(dst.left(8), zeroes);
            QCOMPARE(dst.mid(len + 8, 8), zeroes);
        }
    }
}

void tst_QString::fromLatin1Alternatives() const
{
    QFETCH(FromLatin1Function, function);

    QString dst(fromLatin1Data.maxLength + 16, QChar('x'));
    fromLatin1Alternatives_internal(function, dst, true);

    QBENCHMARK {
        fromLatin1Alternatives_internal(function, dst, false);
    }
}

typedef int (* ToXxxFunction)(uchar *, const ushort *, int);
Q_DECLARE_METATYPE(ToXxxFunction)

#ifdef Q_COMPILER_REF_QUALIFIERS
typedef QByteArray (QString:: *QStringToXxxFunction)() &&;
#else
typedef QByteArray (QString:: *QStringToXxxFunction)() const;
#endif

static void toXxxAlternatives_internal(StringData &toXxxData, ToXxxFunction function, QByteArray &dst, QStringToXxxFunction referenceFunction)
{
    const StringData::Entry *entries = toXxxData.entries;

    for (int i = 0; i < toXxxData.entryCount; ++i) {
        int len = entries[i].len1/2;
        const ushort *src = toXxxData.ushortData + entries[i].offset1/2;

        if (!function)
            continue;
        if (!referenceFunction) {
            (function)(reinterpret_cast<uchar *>(dst.data()), src, len);
        } else {
            dst.fill('x', dst.length());

            int newlen = (function)(reinterpret_cast<uchar *>(dst.data() + 8), src, len);

            QByteArray expected = (QString::fromUtf16(src, len).*referenceFunction)();
            QByteArray final = dst.mid(8, expected.length());
            if (final != expected || newlen != expected.length())
                qDebug() << i << entries[i].offset1 << newlen << expected.length() << QTest::toString(QString::fromUtf16(src, len));

            QCOMPARE(final, expected);
            QCOMPARE(newlen, expected.length());

            QByteArray zeroes(8, 'x');
            QCOMPARE(dst.left(8), zeroes);
            QCOMPARE(dst.mid(expected.length() + 8, 8), zeroes);
        }
    }
}

#ifdef QT_HAVE_ICU
int toLatin1_icu(uchar *dst, const ushort *uc, int len)
{
    UErrorCode err = U_ZERO_ERROR;
    static UConverter *conv = ucnv_open("ISO-8859-1", &err);
    char *b = reinterpret_cast<char *>(dst);
    ucnv_fromUnicode(conv, &b, b + len, &uc, uc + len, NULL, true, &err);
    return len;
}
#endif

int toLatin1_plain(uchar *dst, const ushort *src, int len)
{
    int length = len;
    while (length--) {
        *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        ++src;
    }
    return len;
}

#ifdef __SSE2__
// This is how Qt 4.7 implemented the SSE2 version
int toLatin1_sse2_qt47(uchar *dst, const ushort *src, int len)
{
    int length = len;
    if (length >= 16) {
        const int chunkCount = length >> 4; // divided by 16
        const __m128i questionMark = _mm_set1_epi16('?');
        // SSE has no compare instruction for unsigned comparison.
        // The variables must be shiffted + 0x8000 to be compared
        const __m128i signedBitOffset = _mm_set1_epi16(0x8000);
        const __m128i thresholdMask = _mm_set1_epi16(0xff + 0x8000);
        for (int i = 0; i < chunkCount; ++i) {
            __m128i chunk1 = _mm_loadu_si128((__m128i*)src); // load
            src += 8;
            {
                // each 16 bit is equal to 0xFF if the source is outside latin 1 (>0xff)
                const __m128i signedChunk = _mm_add_epi16(chunk1, signedBitOffset);
                const __m128i offLimitMask = _mm_cmpgt_epi16(signedChunk, thresholdMask);

                // offLimitQuestionMark contains '?' for each 16 bits that was off-limit
                // the 16 bits that were correct contains zeros
                const __m128i offLimitQuestionMark = _mm_and_si128(offLimitMask, questionMark);

                // correctBytes contains the bytes that were in limit
                // the 16 bits that were off limits contains zeros
                const __m128i correctBytes = _mm_andnot_si128(offLimitMask, chunk1);

                // merge offLimitQuestionMark and correctBytes to have the result
                chunk1 = _mm_or_si128(correctBytes, offLimitQuestionMark);
            }

            __m128i chunk2 = _mm_loadu_si128((__m128i*)src); // load
            src += 8;
            {
                // exactly the same operations as for the previous chunk of data
                const __m128i signedChunk = _mm_add_epi16(chunk2, signedBitOffset);
                const __m128i offLimitMask = _mm_cmpgt_epi16(signedChunk, thresholdMask);
                const __m128i offLimitQuestionMark = _mm_and_si128(offLimitMask, questionMark);
                const __m128i correctBytes = _mm_andnot_si128(offLimitMask, chunk2);
                chunk2 = _mm_or_si128(correctBytes, offLimitQuestionMark);
            }

            // pack the two vector to 16 x 8bits elements
            const __m128i result = _mm_packus_epi16(chunk1, chunk2);

            _mm_storeu_si128((__m128i*)dst, result); // store
            dst += 16;
        }
        length = length % 16;
    }

    while (length--) {
        *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        ++src;
    }

    return len;
}

#ifdef __SSE4_1__
// Qt 4.8 had improvements upon the Qt 4.7 code by using SSE4.x
int toLatin1_sse41_qt48(uchar *dst, const ushort *src, int len)
{
    int length = len;
    if (length >= 16) {
        const int chunkCount = length >> 4; // divided by 16
        const __m128i questionMark = _mm_set1_epi16('?');
        // SSE has no compare instruction for unsigned comparison.
        // The variables must be shiffted + 0x8000 to be compared
        const __m128i signedBitOffset = _mm_set1_epi16(0x8000);
        const __m128i thresholdMask = _mm_set1_epi16(0xff + 0x8000);
        for (int i = 0; i < chunkCount; ++i) {
            __m128i chunk1 = _mm_loadu_si128((__m128i*)src); // load
            src += 8;
            {
                // each 16 bit is equal to 0xFF if the source is outside latin 1 (>0xff)
                const __m128i signedChunk = _mm_add_epi16(chunk1, signedBitOffset);
                const __m128i offLimitMask = _mm_cmpgt_epi16(signedChunk, thresholdMask);

                chunk1 = _mm_blendv_epi8(chunk1, questionMark, offLimitMask);
            }

            __m128i chunk2 = _mm_loadu_si128((__m128i*)src); // load
            src += 8;
            {
                // exactly the same operations as for the previous chunk of data
                const __m128i signedChunk = _mm_add_epi16(chunk2, signedBitOffset);
                const __m128i offLimitMask = _mm_cmpgt_epi16(signedChunk, thresholdMask);
                chunk2 = _mm_blendv_epi8(chunk2, questionMark, offLimitMask);
            }

            // pack the two vector to 16 x 8bits elements
            const __m128i result = _mm_packus_epi16(chunk1, chunk2);

            _mm_storeu_si128((__m128i*)dst, result); // store
            dst += 16;
        }
        length = length % 16;
    }

    while (length--) {
        *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        ++src;
    }

    return len;
}

#ifdef __SSE4_2__
int toLatin1_sse42_qt48(uchar *dst, const ushort *src, int len)
{
    int length = len;
    if (length >= 16) {
        const int chunkCount = length >> 4; // divided by 16
        const int mode = _SIDD_UWORD_OPS | _SIDD_CMP_RANGES | _SIDD_UNIT_MASK;
        const __m128i questionMark = _mm_set1_epi16('?');
        const __m128i rangeMatch = _mm_cvtsi32_si128(0xffff0100);

        for (int i = 0; i < chunkCount; ++i) {
            __m128i chunk1 = _mm_loadu_si128((__m128i*)src); // load
            {
                const __m128i offLimitMask = _mm_cmpestrm(rangeMatch, 2, chunk1, 8, mode);

                // replace the non-Latin 1 characters in the chunk with question marks
                chunk1 = _mm_blendv_epi8(chunk1, questionMark, offLimitMask);
            }
            src += 8;

            __m128i chunk2 = _mm_loadu_si128((__m128i*)src); // load
            {
                const __m128i offLimitMask = _mm_cmpestrm(rangeMatch, 2, chunk2, 8, mode);

                // replace the non-Latin 1 characters in the chunk with question marks
                chunk2 = _mm_blendv_epi8(chunk2, questionMark, offLimitMask);
            }
            src += 8;

            // pack the two vector to 16 x 8bits elements
            const __m128i result = _mm_packus_epi16(chunk1, chunk2);

            _mm_storeu_si128((__m128i*)dst, result); // store
            dst += 16;
        }
        length = length % 16;
    }

    while (length--) {
        *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        ++src;
    }

    return len;
}
#endif // __SSE4_2__
#endif // __SSE4_1__
#endif // __SSE2__

#ifdef __ARM_NEON__
static int toLatin1_neon_qt47(uchar *dst, const ushort *src, int len)
{
    int length = len;
    if (length >= 16) {
        const int chunkCount = length >> 3; // divided by 8
        const uint16x8_t questionMark = vdupq_n_u16('?'); // set
        const uint16x8_t thresholdMask = vdupq_n_u16(0xff); // set
        for (int i = 0; i < chunkCount; ++i) {
            uint16x8_t chunk = vld1q_u16((uint16_t *)src); // load
            src += 8;

            const uint16x8_t offLimitMask = vcgtq_u16(chunk, thresholdMask); // chunk > thresholdMask
            const uint16x8_t offLimitQuestionMark = vandq_u16(offLimitMask, questionMark); // offLimitMask & questionMark
            const uint16x8_t correctBytes = vbicq_u16(chunk, offLimitMask); // !offLimitMask & chunk
            chunk = vorrq_u16(correctBytes, offLimitQuestionMark); // correctBytes | offLimitQuestionMark
            const uint8x8_t result = vmovn_u16(chunk); // narrowing move->packing
            vst1_u8(dst, result); // store
            dst += 8;
        }
        length = length % 8;
    }
    while (length--) {
        *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        ++src;
    }

    return len;
}
#endif

void tst_QString::toLatin1Alternatives_data() const
{
    QTest::addColumn<ToXxxFunction>("function");
#ifdef QT_HAVE_ICU
    QTest::newRow("icu") << &toLatin1_icu;
#endif
    QTest::newRow("plain") << &toLatin1_plain;
#ifdef __SSE2__
    QTest::newRow("sse2-qt4.7") << &toLatin1_sse2_qt47;
#ifdef __SSE4_1__
    QTest::newRow("sse4.1-qt4.8") << &toLatin1_sse41_qt48;
#ifdef __SSE4_2__
    QTest::newRow("sse4.2-qt4.8") << &toLatin1_sse42_qt48;
#endif
#endif
#endif
#ifdef __ARM_NEON__
    QTest::newRow("neon-qt4.7") << &toLatin1_neon_qt47;
#endif
}

extern StringData toLatin1Data;
void tst_QString::toLatin1Alternatives() const
{
    QFETCH(ToXxxFunction, function);

    QByteArray dst(toLatin1Data.maxLength + 16, 'x');
    toXxxAlternatives_internal(toLatin1Data, function, dst, &QString::toLatin1); // self-check

    QBENCHMARK {
        toXxxAlternatives_internal(toLatin1Data, function, dst, 0);
    }
}

typedef int (* FromUtf8Function)(ushort *, const char *, int);
Q_DECLARE_METATYPE(FromUtf8Function)

namespace Utf8Sse42 { extern int convertFromUnicode(uchar *buffer, const ushort *uc, int len); }
namespace Utf8Sse41 { extern int convertFromUnicode(uchar *buffer, const ushort *uc, int len); }
namespace Utf8Sse2Alt1 { extern int convertFromUnicode(uchar *buffer, const ushort *uc, int len); }
namespace Utf8Sse2Alt2 { extern int convertFromUnicode(uchar *buffer, const ushort *uc, int len); }
namespace Utf8Sse2 {
extern int convertFromUnicode(uchar *buffer, const ushort *uc, int len);
extern int convertToUnicode(ushort *, const char *, int);
}
namespace Utf8Plain {
extern int convertFromUnicode(uchar *buffer, const ushort *uc, int len);
extern int convertToUnicode(ushort *, const char *, int);
}

extern QTextCodec::ConverterState *state;
QTextCodec::ConverterState *state = 0; // just because the code in qutfcodec.cpp uses a state

int fromUtf8_icu(ushort *dst, const char *src, int len)
{
#ifdef QT_HAVE_ICU
    UErrorCode err = U_ZERO_ERROR;
    static UConverter *conv = ucnv_open("UTF-8", &err);
    ushort *orig_dst = dst;
    ucnv_toUnicode(conv, &dst, dst + len, &src, src + len, NULL, true, &err);
    return dst - orig_dst;
#endif
}

int fromUtf8_qt47(ushort *dst, const char *chars, int len)
{
    // this is almost the code found in Qt 4.7's qutfcodec.cpp QUtf8Codec::convertToUnicode
    // That function returns a QString, this one returns the number of characters converted
    // That's to avoid doing malloc() inside the benchmark test
    // Any differences between this code and the original are just because of that, I promise

    bool headerdone = false;
    ushort replacement = QChar::ReplacementCharacter;
    int need = 0;
    int error = -1;
    uint uc = 0;
    uint min_uc = 0;
    if (state) {
        if (state->flags & QTextCodec::IgnoreHeader)
            headerdone = true;
        if (state->flags & QTextCodec::ConvertInvalidToNull)
            replacement = QChar::Null;
        need = state->remainingChars;
        if (need) {
            uc = state->state_data[0];
            min_uc = state->state_data[1];
        }
    }
    if (!headerdone && len > 3
        && (uchar)chars[0] == 0xef && (uchar)chars[1] == 0xbb && (uchar)chars[2] == 0xbf) {
        // starts with a byte order mark
        chars += 3;
        len -= 3;
        headerdone = true;
    }

    // QString result(need + len + 1, Qt::Uninitialized); // worst case
    // ushort *qch = (ushort *)result.unicode();
    ushort *qch = dst;
    uchar ch;
    int invalid = 0;

    for (int i = 0; i < len; ++i) {
        ch = chars[i];
        if (need) {
            if ((ch&0xc0) == 0x80) {
                uc = (uc << 6) | (ch & 0x3f);
                --need;
                if (!need) {
                    // utf-8 bom composes into 0xfeff code point
                    if (!headerdone && uc == 0xfeff) {
                        // don't do anything, just skip the BOM
                    } else if (QChar::requiresSurrogates(uc) && uc <= QChar::LastValidCodePoint) {
                        // surrogate pair
                        //Q_ASSERT((qch - (ushort*)result.unicode()) + 2 < result.length());
                        *qch++ = QChar::highSurrogate(uc);
                        *qch++ = QChar::lowSurrogate(uc);
                    } else if ((uc < min_uc) || QChar::isSurrogate(uc) || uc > QChar::LastValidCodePoint) {
                        // error: overlong sequence or UTF16 surrogate
                        *qch++ = replacement;
                        ++invalid;
                    } else {
                        *qch++ = uc;
                    }
                    headerdone = true;
                }
            } else {
                // error
                i = error;
                *qch++ = replacement;
                ++invalid;
                need = 0;
                headerdone = true;
            }
        } else {
            if (ch < 128) {
                *qch++ = ushort(ch);
                headerdone = true;
            } else if ((ch & 0xe0) == 0xc0) {
                uc = ch & 0x1f;
                need = 1;
                error = i;
                min_uc = 0x80;
                headerdone = true;
            } else if ((ch & 0xf0) == 0xe0) {
                uc = ch & 0x0f;
                need = 2;
                error = i;
                min_uc = 0x800;
            } else if ((ch&0xf8) == 0xf0) {
                uc = ch & 0x07;
                need = 3;
                error = i;
                min_uc = 0x10000;
                headerdone = true;
            } else {
                // error
                *qch++ = replacement;
                ++invalid;
                headerdone = true;
            }
        }
    }
    if (!state && need > 0) {
        // unterminated UTF sequence
        for (int i = error; i < len; ++i) {
            *qch++ = replacement;
            ++invalid;
        }
    }
    //result.truncate(qch - (ushort *)result.unicode());
    if (state) {
        state->invalidChars += invalid;
        state->remainingChars = need;
        if (headerdone)
            state->flags |= QTextCodec::IgnoreHeader;
        state->state_data[0] = need ? uc : 0;
        state->state_data[1] = need ? min_uc : 0;
    }
    //return result;
    return qch - dst;
}

#if defined(__ARM_NEON__) && (defined(Q_PROCESSOR_ARM_V7) || defined(__ARM_ARCH_6T2__))
int fromUtf8_latin1_neon(ushort *dst, const char *chars, int len)
{
    fromLatin1_neon_improved(dst, chars, len);
    return len;
}

int fromUtf8_neon(ushort *qch, const char *chars, int len)
{
    if (len > 3
        && (uchar)chars[0] == 0xef && (uchar)chars[1] == 0xbb && (uchar)chars[2] == 0xbf) {
        // starts with a byte order mark
        chars += 3;
        len -= 3;
    }

    ushort *dst = qch;
    const uint8x8_t highBit = vdup_n_u8(0x80);
    while (len >= 8) {
        // load 8 bytes into one doubleword Neon register
        const uint8x8_t chunk = vld1_u8((uint8_t *)chars);
        const uint16x8_t expanded = vmovl_u8(chunk);
        vst1q_u16(dst, expanded);

        uint8x8_t highBits = vtst_u8(chunk, highBit);
        // we need to find the lowest byte set
        int mask_low = vget_lane_u32(vreinterpret_u32_u8(highBits), 0);
        int mask_high = vget_lane_u32(vreinterpret_u32_u8(highBits), 1);

        if (__builtin_expect(mask_low == 0 && mask_high == 0, 1)) {
            chars += 8;
            dst += 8;
            len -= 8;
        } else {
            // UTF-8 character found
            // which one?
            qptrdiff pos;
            asm ("rbit  %0, %1\n"
                 "clz   %1, %1\n"
               : "=r" (pos)
               : "r" (mask_low ? mask_low : mask_high));
            // now mask_low contains the number of leading zeroes
            // or the value 32 (0x20) if no zeroes were found
            // the number of leading zeroes is 8*pos
            pos /= 8;

            extract_utf8_multibyte<false>(dst, chars, pos, len);
            chars += pos;
            dst += pos;
            len -= pos;
        }
    }

    qptrdiff counter = 0;
    while (counter < len) {
        uchar ch = chars[counter];
        if ((ch & 0x80) == 0) {
            dst[counter] = ch;
            ++counter;
            continue;
        }
        // UTF-8 character found
        extract_utf8_multibyte<false>(dst, chars, counter, len);
    }
    return dst + counter - qch;
}

int fromUtf8_neon_trusted(ushort *qch, const char *chars, int len)
{
    ushort *dst = qch;
    const uint8x8_t highBit = vdup_n_u8(0x80);
    while (len >= 8) {
        // load 8 bytes into one doubleword Neon register
        const uint8x8_t chunk = vld1_u8((uint8_t *)chars);
        const uint16x8_t expanded = vmovl_u8(chunk);
        vst1q_u16(dst, expanded);

        uint8x8_t highBits = vtst_u8(chunk, highBit);
        // we need to find the lowest byte set
        int mask_low = vget_lane_u32(vreinterpret_u32_u8(highBits), 0);
        int mask_high = vget_lane_u32(vreinterpret_u32_u8(highBits), 1);

        if (__builtin_expect(mask_low == 0 && mask_high == 0, 1)) {
            chars += 8;
            dst += 8;
            len -= 8;
        } else {
            // UTF-8 character found
            // which one?
            qptrdiff pos;
            asm ("rbit  %0, %1\n"
                 "clz   %1, %1\n"
               : "=r" (pos)
               : "r" (mask_low ? mask_low : mask_high));
            // now mask_low contains the number of leading zeroes
            // or the value 32 (0x20) if no zeroes were found
            // the number of leading zeroes is 8*pos
            pos /= 8;

            extract_utf8_multibyte<true>(dst, chars, pos, len);
            chars += pos;
            dst += pos;
            len -= pos;
        }
    }

    qptrdiff counter = 0;
    while (counter < len) {
        uchar ch = chars[counter];
        if ((ch & 0x80) == 0) {
            dst[counter] = ch;
            ++counter;
            continue;
        }

        // UTF-8 character found
        extract_utf8_multibyte<true>(dst, chars, counter, len);
    }
    return dst + counter - qch;
}
#endif

void tst_QString::fromUtf8Alternatives_data() const
{
    QTest::addColumn<FromUtf8Function>("function");
//    QTest::newRow("empty") << FromUtf8Function(0);
    QTest::newRow("icu") << &fromUtf8_icu;
    QTest::newRow("qt-4.7") << &fromUtf8_qt47;
    QTest::newRow("qt-5.3") << &Utf8Plain::convertToUnicode;
#ifdef __SSE2__
    QTest::newRow("qt-5.3-sse2") << &Utf8Sse2::convertToUnicode;
#endif
#if defined(__ARM_NEON__) && (defined(Q_PROCESSOR_ARM_V7) || defined(__ARM_ARCH_6T2__))
    QTest::newRow("neon") << &fromUtf8_neon;
    QTest::newRow("neon-trusted-no-bom") << &fromUtf8_neon_trusted;
    QTest::newRow("latin1-neon-improved") << &fromUtf8_latin1_neon;
#endif
}

static void fromUtf8Alternatives_internal(const StringData &fromUtf8Data, FromUtf8Function function, QString &dst, bool doVerify)
{
    struct Entry
    {
        int len;
        int offset1, offset2;
        int align1, align2;
    };
    const Entry *entries = reinterpret_cast<const Entry *>(fromUtf8Data.entries);

    for (int i = 0; i < fromUtf8Data.entryCount; ++i) {
        int len = entries[i].len;
        const char *src = fromUtf8Data.charData + entries[i].offset1;

        if (!function)
            continue;
        if (!doVerify) {
            (function)(&dst.data()->unicode(), src, len);
        } else {
            dst.fill(QChar('x'), dst.length());

            int utf8len = (function)(&dst.data()->unicode() + 8, src, len);

            QString expected = QString::fromUtf8(src, len);
            QString final = dst.mid(8, expected.length());
            if (final != expected || utf8len != expected.length())
                qDebug() << i << entries[i].offset1 << utf8len << final << expected.length() << expected;

            QCOMPARE(final, expected);
            QCOMPARE(utf8len, expected.length());

            QString zeroes(8, QChar('x'));
            QCOMPARE(dst.left(8), zeroes);
            QCOMPARE(dst.mid(len + 8, 8), zeroes);
        }
    }
}

void tst_QString::fromUtf8Alternatives() const
{
    extern StringData fromUtf8Data;
    QFETCH(FromUtf8Function, function);

    QString dst(fromUtf8Data.maxLength + 16, QChar('x'));
    fromUtf8Alternatives_internal(fromUtf8Data, function, dst, true);

    QBENCHMARK {
        fromUtf8Alternatives_internal(fromUtf8Data, function, dst, false);
    }
}

void tst_QString::fromUtf8Alternatives2_data() const
{
    extern StringData fromUtf8_2char, fromUtf8_3char, fromUtf8_4char, fromUtf8_asciiOnly;
    QTest::addColumn<FromUtf8Function>("function");
    QTest::addColumn<StringData *>("data");
//    QTest::newRow("empty") << FromUtf8Function(0) << &fromUtf8_asciiOnly;
    QTest::newRow("ascii:icu") << &fromUtf8_icu << &fromUtf8_asciiOnly;
    QTest::newRow("2char:icu") << &fromUtf8_icu << &fromUtf8_2char;
    QTest::newRow("3char:icu") << &fromUtf8_icu << &fromUtf8_3char;
    QTest::newRow("4char:icu") << &fromUtf8_icu << &fromUtf8_4char;
    QTest::newRow("ascii:qt-4.7") << &fromUtf8_qt47 << &fromUtf8_asciiOnly;
    QTest::newRow("2char:qt-4.7") << &fromUtf8_qt47 << &fromUtf8_2char;
    QTest::newRow("3char:qt-4.7") << &fromUtf8_qt47 << &fromUtf8_3char;
    QTest::newRow("4char:qt-4.7") << &fromUtf8_qt47 << &fromUtf8_4char;
    QTest::newRow("ascii:qt-5.3") << &Utf8Plain::convertToUnicode << &fromUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3") << &Utf8Plain::convertToUnicode << &fromUtf8_2char;
    QTest::newRow("3char:qt-5.3") << &Utf8Plain::convertToUnicode << &fromUtf8_3char;
    QTest::newRow("4char:qt-5.3") << &Utf8Plain::convertToUnicode << &fromUtf8_4char;
#ifdef __SSE2__
    QTest::newRow("ascii:qt-5.3-sse2") << &Utf8Sse2::convertToUnicode << &fromUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3-sse2") << &Utf8Sse2::convertToUnicode << &fromUtf8_2char;
    QTest::newRow("3char:qt-5.3-sse2") << &Utf8Sse2::convertToUnicode << &fromUtf8_3char;
    QTest::newRow("4char:qt-5.3-sse2") << &Utf8Sse2::convertToUnicode << &fromUtf8_4char;
#endif
}

void tst_QString::fromUtf8Alternatives2() const
{
    QFETCH(FromUtf8Function, function);
    QFETCH(StringData *, data);

    QString dst(data->maxLength + 16, QChar('x'));
    fromUtf8Alternatives_internal(*data, function, dst, true);

    QBENCHMARK {
        fromUtf8Alternatives_internal(*data, function, dst, false);
    }
}

int toUtf8_icu(uchar *dst, const ushort *uc, int len)
{
#ifdef QT_HAVE_ICU
    UErrorCode err = U_ZERO_ERROR;
    static UConverter *conv = ucnv_open("UTF-8", &err);
    char *b = reinterpret_cast<char *>(dst);
    ucnv_fromUnicode(conv, &b, b + len * 3, &uc, uc + len, NULL, true, &err);
    return b - reinterpret_cast<char *>(dst);
#endif
}

int toUtf8_qt47(uchar *dst, const ushort *uc, int len)
{
    uchar *cursor = dst;
    uchar replacement = '?';
    int rlen = 3*len;
    int surrogate_high = -1;
    if (state) {
        if (state->flags & QTextCodec::ConvertInvalidToNull)
            replacement = 0;
        if (!(state->flags & QTextCodec::IgnoreHeader))
            rlen += 3;
        if (state->remainingChars)
            surrogate_high = state->state_data[0];
    }

    const QChar *ch = reinterpret_cast<const QChar *>(uc);
    int invalid = 0;
    if (state && !(state->flags & QTextCodec::IgnoreHeader)) {
        *cursor++ = 0xef;
        *cursor++ = 0xbb;
        *cursor++ = 0xbf;
    }

    const QChar *end = ch + len;
    while (ch < end) {
        uint u = ch->unicode();
        if (surrogate_high >= 0) {
            if (ch->isLowSurrogate()) {
                u = QChar::surrogateToUcs4(surrogate_high, u);
                surrogate_high = -1;
            } else {
                // high surrogate without low
                *cursor = replacement;
                ++ch;
                ++invalid;
                surrogate_high = -1;
                continue;
            }
        } else if (ch->isLowSurrogate()) {
            // low surrogate without high
            *cursor = replacement;
            ++ch;
            ++invalid;
            continue;
        } else if (ch->isHighSurrogate()) {
            surrogate_high = u;
            ++ch;
            continue;
        }

        if (u < 0x80) {
            *cursor++ = (uchar)u;
        } else {
            if (u < 0x0800) {
                *cursor++ = 0xc0 | ((uchar) (u >> 6));
            } else {
                if (QChar::requiresSurrogates(u)) {
                    *cursor++ = 0xf0 | ((uchar) (u >> 18));
                    *cursor++ = 0x80 | (((uchar) (u >> 12)) & 0x3f);
                } else {
                    *cursor++ = 0xe0 | (((uchar) (u >> 12)) & 0x3f);
                }
                *cursor++ = 0x80 | (((uchar) (u >> 6)) & 0x3f);
            }
            *cursor++ = 0x80 | ((uchar) (u&0x3f));
        }
        ++ch;
    }

    if (state) {
        state->invalidChars += invalid;
        state->flags |= QTextCodec::IgnoreHeader;
        state->remainingChars = 0;
        if (surrogate_high >= 0) {
            state->remainingChars = 1;
            state->state_data[0] = surrogate_high;
        }
    }
    return cursor - dst;
}


void tst_QString::toUtf8Alternatives_data() const
{
    QTest::addColumn<ToXxxFunction>("function");
//    QTest::newRow("empty") << ToXxxFunction(0);
    QTest::newRow("icu") << &toUtf8_icu;
    QTest::newRow("qt-4.7") << &toUtf8_qt47;
    QTest::newRow("qt-5.3") << &Utf8Plain::convertFromUnicode;
#ifdef __SSE2__
    QTest::newRow("qt-5.3-sse2") << &Utf8Sse2::convertFromUnicode;
    QTest::newRow("qt-5.3-sse2-alt1") << &Utf8Sse2Alt1::convertFromUnicode;
    QTest::newRow("qt-5.3-sse2-alt2") << &Utf8Sse2Alt2::convertFromUnicode;
#endif
#ifdef __SSE4_1__
    QTest::newRow("qt-5.3-sse4.1") << &Utf8Sse41::convertFromUnicode;
#endif
#ifdef __SSE4_2__
    QTest::newRow("qt-5.3-sse4.2") << &Utf8Sse42::convertFromUnicode;
#endif
}

void tst_QString::toUtf8Alternatives() const
{
    extern StringData toUtf8Data;
    QFETCH(ToXxxFunction, function);

    QByteArray dst(toUtf8Data.maxLength * 3 + 16, 'x');
//    toXxxAlternatives_internal(toUtf8Data, function, dst, &QString::toUtf8);

    QBENCHMARK {
        toXxxAlternatives_internal(toUtf8Data, function, dst, 0);
    }
}

void tst_QString::toUtf8Alternatives2_data() const
{
    extern StringData toUtf8_2char, toUtf8_3char, toUtf8_4char, toUtf8_asciiOnly;
    QTest::addColumn<ToXxxFunction>("function");
    QTest::addColumn<StringData *>("data");
//    QTest::newRow("empty") << ToXxxFunction(0) << &toUtf8_asciiOnly;
    QTest::newRow("ascii:icu") << &toUtf8_icu << &toUtf8_asciiOnly;
    QTest::newRow("2char:icu") << &toUtf8_icu << &toUtf8_2char;
    QTest::newRow("3char:icu") << &toUtf8_icu << &toUtf8_3char;
    QTest::newRow("4char:icu") << &toUtf8_icu << &toUtf8_4char;
    QTest::newRow("ascii:qt-4.7") << &toUtf8_qt47 << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-4.7") << &toUtf8_qt47 << &toUtf8_2char;
    QTest::newRow("3char:qt-4.7") << &toUtf8_qt47 << &toUtf8_3char;
    QTest::newRow("4char:qt-4.7") << &toUtf8_qt47 << &toUtf8_4char;
    QTest::newRow("ascii:qt-5.3") << &Utf8Plain::convertFromUnicode << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3") << &Utf8Plain::convertFromUnicode << &toUtf8_2char;
    QTest::newRow("3char:qt-5.3") << &Utf8Plain::convertFromUnicode << &toUtf8_3char;
    QTest::newRow("4char:qt-5.3") << &Utf8Plain::convertFromUnicode << &toUtf8_4char;
#ifdef __SSE2__
    QTest::newRow("ascii:qt-5.3-sse2") << &Utf8Sse2::convertFromUnicode << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3-sse2") << &Utf8Sse2::convertFromUnicode << &toUtf8_2char;
    QTest::newRow("4char:qt-5.3-sse2") << &Utf8Sse2::convertFromUnicode << &toUtf8_4char;
    QTest::newRow("3char:qt-5.3-sse2") << &Utf8Sse2::convertFromUnicode << &toUtf8_3char;
    QTest::newRow("ascii:qt-5.3-sse2-alt1") << &Utf8Sse2Alt1::convertFromUnicode << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3-sse2-alt1") << &Utf8Sse2Alt1::convertFromUnicode << &toUtf8_2char;
    QTest::newRow("3char:qt-5.3-sse2-alt1") << &Utf8Sse2Alt1::convertFromUnicode << &toUtf8_3char;
    QTest::newRow("4char:qt-5.3-sse2-alt1") << &Utf8Sse2Alt1::convertFromUnicode << &toUtf8_4char;
    QTest::newRow("ascii:qt-5.3-sse2-alt2") << &Utf8Sse2Alt2::convertFromUnicode << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3-sse2-alt2") << &Utf8Sse2Alt2::convertFromUnicode << &toUtf8_2char;
    QTest::newRow("3char:qt-5.3-sse2-alt2") << &Utf8Sse2Alt2::convertFromUnicode << &toUtf8_3char;
    QTest::newRow("4char:qt-5.3-sse2-alt2") << &Utf8Sse2Alt2::convertFromUnicode << &toUtf8_4char;
#endif
#ifdef __SSE4_1__
    QTest::newRow("ascii:qt-5.3-sse4.1") << &Utf8Sse41::convertFromUnicode << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3-sse4.1") << &Utf8Sse41::convertFromUnicode << &toUtf8_2char;
    QTest::newRow("3char:qt-5.3-sse4.1") << &Utf8Sse41::convertFromUnicode << &toUtf8_3char;
    QTest::newRow("4char:qt-5.3-sse4.1") << &Utf8Sse41::convertFromUnicode << &toUtf8_4char;
#endif
#ifdef __SSE4_2__
    QTest::newRow("ascii:qt-5.3-sse4.2") << &Utf8Sse42::convertFromUnicode << &toUtf8_asciiOnly;
    QTest::newRow("2char:qt-5.3-sse4.2") << &Utf8Sse42::convertFromUnicode << &toUtf8_2char;
    QTest::newRow("3char:qt-5.3-sse4.2") << &Utf8Sse42::convertFromUnicode << &toUtf8_3char;
    QTest::newRow("4char:qt-5.3-sse4.2") << &Utf8Sse42::convertFromUnicode << &toUtf8_4char;
#endif
}

void tst_QString::toUtf8Alternatives2() const
{
    QFETCH(ToXxxFunction, function);
    QFETCH(StringData *, data);

    QByteArray dst(data->maxLength * 3 + 16, 'x');
    toXxxAlternatives_internal(*data, function, dst, &QString::toUtf8);

    QBENCHMARK {
        toXxxAlternatives_internal(*data, function, dst, 0);
    }
}

static int findChar_qt52(const ushort *str, ushort c, int len)
{
    int from = 0; // not passed here, str is already adjusted

    const ushort *s = (const ushort *)str;
    const ushort *n = s + from - 1;
    const ushort *e = s + len;
    while (++n != e)
        if (*n == c)
            return  n - s;
    return -1;
}

#ifdef __SSE2__
static int findChar_short_tail(const ushort *n, const ushort *s, ushort c, int len)
{
    if (!len--)
        return -1;
    if (n[0] == c)
        return n - s + 0;
    if (!len--)
        return -1;
    if (n[1] == c)
        return n - s + 1;
    if (!len--)
        return -1;
    if (n[2] == c)
        return n - s + 2;
    if (!len--)
        return -1;
    if (n[3] == c)
        return n - s + 3;
    if (!len--)
        return -1;
    if (n[4] == c)
        return n - s + 4;
    if (!len--)
        return -1;
    if (n[5] == c)
        return n - s + 5;
    if (!len--)
        return -1;
    if (n[6] == c)
        return n - s + 6;
    if (!len--)
        return -1;
    if (n[7] == c)
        return n - s + 7;
    return -1;
}

static int findChar_qt53(const ushort *str, ushort c, int len)
{
    int from = 0; // not passed here, str is already adjusted

    const ushort *s = (const ushort *)str;
    const ushort *n = s + from;
    const ushort *e = s + len;

    __m128i mch = _mm_set1_epi32(c | (c << 16));

    // we're going to read n[0..7] (16 bytes)
    for (const ushort *next = n + 8; next <= e; n = next, next += 8) {
        __m128i data = _mm_loadu_si128((__m128i*)n);
        __m128i result = _mm_cmpeq_epi16(data, mch);
        ushort mask = _mm_movemask_epi8(result);
        if (mask) {
            // found a match
            // same as: return n - s + _bit_scan_forward(mask) / 2
            return (reinterpret_cast<const char *>(n) - reinterpret_cast<const char *>(s)
                    + _bit_scan_forward(mask)) >> 1;
        }
    }

    // unrolled tail loop
    return findChar_short_tail(n, s, c, e - n);
}

#ifdef __SSE4_2__
static int findChar_sse42(const ushort *str, ushort c, int len)
{
    int from = 0; // not passed here, str is already adjusted

    const ushort *s = (const ushort *)str;
    const ushort *n = s + from;
    const ushort *e = s + len;

    // search 16-bit unsigned data (UTF-16) for any character match
    // using postive polarity (we want to get the first match).
    // The instruction sets CF = 0 if no match is found
    enum { Mode = _SIDD_UWORD_OPS | _SIDD_CMP_EQUAL_ANY | _SIDD_POSITIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT };
    __m128i mch = _mm_cvtsi32_si128(c);

    // we're going to read n[0..7] (16 bytes)
    for (const ushort *next = n + 8; next <= e; n = next, next += 8) {
        __m128i data = _mm_loadu_si128((__m128i*)n);
        bool cmp = _mm_cmpestrc(mch, 1, data, 8, Mode);
        if (cmp) {
            // found a match
            uint idx = _mm_cmpestri(mch, 1, data, 8, Mode);
            return idx + n - s;
        }
    }

    // unrolled tail loop
    return findChar_short_tail(n, s, c, e - n);
}

#endif // __SSE4_2__
#endif // __SSE2__

typedef int (* FindCharFunction)(const ushort *, ushort, int);
Q_DECLARE_METATYPE(FindCharFunction)
extern StringData findCharData;
void tst_QString::findChar_data()
{
    QTest::addColumn<FindCharFunction>("function");
    QTest::newRow("qt-5.2") << &findChar_qt52;
#ifdef __SSE2__
    QTest::newRow("qt-5.3") << &findChar_qt53;
#ifdef __SSE4_2__
    QTest::newRow("sse4.2") << &findChar_sse42;
#endif
#endif
}

void tst_QString::findChar()
{
    QFETCH(FindCharFunction, function);

    const StringData::Entry *entries = reinterpret_cast<const StringData::Entry *>(findCharData.entries);
    QBENCHMARK {
        for (int i = 0; i < findCharData.entryCount; ++i) {
            int len = entries[i].len1/2;
            const ushort *src = findCharData.ushortData + entries[i].offset1/2;
            ushort ch = entries[i].align2; // we store the char to find in the alignment field

            (function)(src, ch, len);
        }
    }
}

void tst_QString::findCharCps_data()
{
    findChar_data();
}

void tst_QString::findCharCps()
{
    // same as findChar, but report in characters scanned per second
    QFETCH(FindCharFunction, function);

    const StringData::Entry *entries = reinterpret_cast<const StringData::Entry *>(findCharData.entries);
    qint64 totalBytes = 0;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 500 && totalBytes < std::numeric_limits<qint64>::max() / 4) {
        for (int i = 0; i < findCharData.entryCount; ++i) {
            int len = entries[i].len1/2;
            const ushort *src = findCharData.ushortData + entries[i].offset1/2;
            ushort ch = entries[i].align2; // we store the char to find in the alignment field

            int n = (function)(src, ch, len);
            if (n == -1)
                totalBytes += len;
            else
                totalBytes += n;
        }
    }
    QTest::setBenchmarkResult(totalBytes / (timer.nsecsElapsed() / 1e9) * 2, QTest::BytesPerSecond);
}

void tst_QString::toUpper_data()
{
    QTest::addColumn<QString>("s");

    QString lowerLatin1(300, QChar('a'));
    QString upperLatin1(300, QChar('A'));

    QString lowerDeseret;
    {
        QString pattern;
        pattern += QChar(QChar::highSurrogate(0x10428));
        pattern += QChar(QChar::lowSurrogate(0x10428));
        for (int i = 0; i < 300 / pattern.size(); ++i)
            lowerDeseret += pattern;
    }
    QString upperDeseret;
    {
        QString pattern;
        pattern += QChar(QChar::highSurrogate(0x10400));
        pattern += QChar(QChar::lowSurrogate(0x10400));
        for (int i = 0; i < 300 / pattern.size(); ++i)
            upperDeseret += pattern;
    }

    QString lowerLigature(600, QChar(0xFB03));

    QTest::newRow("600<a>") << (lowerLatin1 + lowerLatin1);
    QTest::newRow("600<A>") << (upperLatin1 + upperLatin1);

    QTest::newRow("300<a>+300<A>") << (lowerLatin1 + upperLatin1);
    QTest::newRow("300<A>+300<a>") << (upperLatin1 + lowerLatin1);

    QTest::newRow("300<10428>") << (lowerDeseret + lowerDeseret);
    QTest::newRow("300<10400>") << (upperDeseret + upperDeseret);

    QTest::newRow("150<10428>+150<10400>") << (lowerDeseret + upperDeseret);
    QTest::newRow("150<10400>+150<10428>") << (upperDeseret + lowerDeseret);

    QTest::newRow("300a+150<10400>") << (lowerLatin1 + upperDeseret);
    QTest::newRow("300a+150<10428>") << (lowerLatin1 + lowerDeseret);
    QTest::newRow("300A+150<10400>") << (upperLatin1 + upperDeseret);
    QTest::newRow("300A+150<10428>") << (upperLatin1 + lowerDeseret);

    QTest::newRow("600<FB03> (ligature)") << lowerLigature;
}

void tst_QString::toUpper()
{
    QFETCH(QString, s);

    QBENCHMARK {
        s.toUpper();
    }
}

void tst_QString::toLower_data()
{
    toUpper_data();
}

void tst_QString::toLower()
{
    QFETCH(QString, s);

    QBENCHMARK {
        s.toLower();
    }
}

void tst_QString::toCaseFolded_data()
{
    toUpper_data();
}

void tst_QString::toCaseFolded()
{
    QFETCH(QString, s);

    QBENCHMARK {
        s.toCaseFolded();
    }
}

QTEST_APPLESS_MAIN(tst_QString)

#include "main.moc"
