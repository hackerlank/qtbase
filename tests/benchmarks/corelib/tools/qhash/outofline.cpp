/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtTest module of the Qt Toolkit.
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

#include "main.h"
#include <qendian.h>
#include <private/qsimd_p.h>

QT_BEGIN_NAMESPACE

uint qHash(const Qt4String &str)
{
    int n = str.length();
    const QChar *p = str.unicode();
    uint h = 0;

    while (n--) {
        h = (h << 4) + (*p++).unicode();
        h ^= (h & 0xf0000000) >> 23;
        h &= 0x0fffffff;
    }
    return h;
}

uint qHash(const Qt50String &key, uint seed)
{
    const QChar *p = key.unicode();
    int len = key.size();
    uint h = seed;
    for (int i = 0; i < len; ++i)
        h = 31 * h + p[i].unicode();
    return h;
}

// The Java's hashing algorithm for strings is a variation of D. J. Bernstein
// hashing algorithm appeared here http://cr.yp.to/cdb/cdb.txt
// and informally known as DJB33XX - DJB's 33 Times Xor.
// Java uses DJB31XA, that is, 31 Times Add.
// The original algorithm was a loop around  "(h << 5) + h ^ c",
// which is indeed "h * 33 ^ c"; it was then changed to
// "(h << 5) - h ^ c", so "h * 31 ^ c", and the XOR changed to a sum:
// "(h << 5) - h + c", which can save some assembly instructions.
// Still, we can avoid writing the multiplication as "(h << 5) - h"
// -- the compiler will turn it into a shift and an addition anyway
// (for instance, gcc 4.4 does that even at -O0).
uint qHash(const JavaString &str)
{
    const unsigned short *p = (unsigned short *)str.constData();
    const int len = str.size();

    uint h = 0;

    for (int i = 0; i < len; ++i)
        h = 31 * h + p[i];

    return h;
}

#if QT_COMPILER_SUPPORTS_HERE(SSE4_2)
template <typename Char>
QT_FUNCTION_TARGET(SSE4_2)
static uint crc32(const Char *ptr, size_t len, uint h)
{
    // The CRC32 instructions from Nehalem calculate a 32-bit CRC32 checksum
    const uchar *p = reinterpret_cast<const uchar *>(ptr);
    const uchar *const e = p + (len * sizeof(Char));
#  ifdef Q_PROCESSOR_X86_64
    // The 64-bit instruction still calculates only 32-bit, but without this
    // variable GCC 4.9 still tries to clear the high bits on every loop
    qulonglong h2 = h;

    p += 8;
    for ( ; p <= e; p += 8)
        h2 = _mm_crc32_u64(h2, qFromUnaligned<qlonglong>(p - 8));
    h = h2;
    p -= 8;

    len = e - p;
    if (len & 4) {
        h = _mm_crc32_u32(h, qFromUnaligned<uint>(p));
        p += 4;
    }
#  else
    p += 4;
    for ( ; p <= e; p += 4)
        h = _mm_crc32_u32(h, qFromUnaligned<uint>(p - 4));
    p -= 4;
    len = e - p;
#  endif
    if (len & 2) {
        h = _mm_crc32_u16(h, qFromUnaligned<ushort>(p));
        p += 2;
    }
    if (sizeof(Char) == 1 && len & 1)
        h = _mm_crc32_u8(h, *p);
    return h;
}

uint qHash(const Crc32String &str, uint seed)
{
    return crc32(str.constData(), str.size(), seed);
}

#endif

// Disable extra optimizations, particularly the vectorizers
#if defined(Q_CC_INTEL) && !defined(Q_CC_MSVC)
#  pragma GCC optimization_level 2
#elif defined(Q_CC_GNU)
#  pragma GCC optimize("O2")
#endif

// Our 128-bit is composed of two identical 64-bit halves, each composed of
// the 32-bit of the seed and twice the low 16-bit of the length.
#if QT_COMPILER_SUPPORTS_HERE(SSE2)
typedef __m128i Key128;

QT_FUNCTION_TARGET(SSE2)
static void makeKey128(Key128 &key, uint len, uint seed)
{
    // same as:
//    const __m128i key = _mm_setr_epi16(seed, seed >> 16, len, len, seed, seed >> 16, len, len);
//    __m128i key = _mm_setr_epi32(seed, (len & 0xffff) | int((len & 0xffffu) << 16), 0, 0);
//    key = _mm_unpacklo_epi64(key, key);
    uint replicated_len = (len & 0xffff) | int((len & 0xffffu) << 16);
    __m128i mseed = _mm_cvtsi32_si128(seed);
#  ifdef __SSE4_1__
    key = _mm_insert_epi32(mseed, replicated_len, 1);
#  else
    __m128i mlen = _mm_cvtsi32_si128(replicated_len);
    mlen = _mm_slli_si128(mlen, 4);
    key = _mm_or_si128(mseed, mlen);
#  endif
    key = _mm_unpacklo_epi64(key, key);
}
#else
struct Key128 { quint32 d[4]; };

static void makeKey128(Key128 &result, int len, uint seed)
{
    result.d[0] = result.d[2] = seed;
    result.d[1] = result.d[3] = (len & 0xffff) | int((len & 0xffffu) << 16);
}
#endif

#if QT_COMPILER_SUPPORTS_HERE(AES) && QT_COMPILER_SUPPORTS_HERE(SSE4_2)
QT_FUNCTION_TARGET(AES)
static inline qregisteruint aeshash(const uchar *p, uint len, uint seed) Q_DECL_NOTHROW
{
    // This is inspired by the algorithm in the Go language. See:
    // https://github.com/golang/go/blob/894abb5f680c040777f17f9f8ee5a5ab3a03cb94/src/runtime/asm_386.s#L902
    // https://github.com/golang/go/blob/894abb5f680c040777f17f9f8ee5a5ab3a03cb94/src/runtime/asm_amd64.s#L903
    const auto hash16bytes = [](__m128i &state0, __m128i data) {
        __m128i s0 = _mm_unpacklo_epi8(state0, data);   // mix the state and the data
        __m128i s1 = _mm_unpackhi_epi8(state0, data);
        s0 = _mm_aesenc_si128(s0, state0);
        s0 = _mm_aesenc_si128(s0, state0);
        s1 = _mm_aesenc_si128(s1, state0);
        s1 = _mm_aesenc_si128(s1, state0);
        state0 = _mm_xor_si128(s0, s1);
    };

    __m128i key;
    makeKey128(key, len, seed);
    __m128i state0 = key;
    auto src = reinterpret_cast<const __m128i *>(p);

    if (len < 16)
        goto lt16;
    if (len < 32)
        goto lt32;

    // rounds of 32 bytes
    {
        // state1 = ~state0;
        __m128i any = _mm_undefined_si128();
        __m128i one = _mm_cmpeq_epi64(any, any);
        __m128i state1 = _mm_xor_si128(state0, one);

        // do simplified rounds of 32 bytes, keeping 256 bits of state
        const auto srcend = src + (len / 32);
        while (src < srcend) {
            __m128i data0 = _mm_loadu_si128(src);
            __m128i data1 = _mm_loadu_si128(src + 1);
            data0 = _mm_xor_si128(data0, state0);
            data1 = _mm_xor_si128(data1, state1);
            state0 = _mm_aesenc_si128(state0, data0);
            state0 = _mm_aesenc_si128(state0, data0);
            state1 = _mm_aesenc_si128(state1, data1);
            state1 = _mm_aesenc_si128(state1, data1);
            src += 2;
        }
        state0 = _mm_xor_si128(state0, state1);
    }

    // do we still have 16 or more bytes?
    if (len & 0x10) {
lt32:
        __m128i data = _mm_loadu_si128(src);
        hash16bytes(state0, data);
        ++src;
    }
    len &= 0xf;

lt16:
    if (len) {
        // load the last chunk of data
        // We're going to load 16 bytes and mask zero the part we don't care
        // (the hash of a short string is different from the hash of a longer
        // including NULLs at the end because the length is in the key)
        // WARNING: this may produce valgrind warnings, but it's safe

        __m128i data;

        if (Q_LIKELY(quintptr(src + 1) & 0xff0)) {
            // same page, we definitely can't fault:
            // load all 16 bytes and mask off the bytes past the end of the source
            static const qint8 maskarray[] = {
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
            };
            __m128i mask = _mm_loadu_si128(reinterpret_cast<const __m128i *>(maskarray + 15 - len));
            data = _mm_loadu_si128(src);
            data = _mm_and_si128(data, mask);
        } else {
            // too close to the end of the page, it could fault:
            // load 16 bytes ending at the data end, then shuffle them to the beginning
            static const qint8 shufflecontrol[] = {
                 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
            };
            __m128i control = _mm_loadu_si128(reinterpret_cast<const __m128i *>(shufflecontrol + 15 - len));
            p = reinterpret_cast<const uchar *>(src - 1);
            data = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p + len));
            data = _mm_shuffle_epi8(data, control);
        }

        hash16bytes(state0, data);
    }

    // extract state0
#  ifdef Q_PROCESSOR_X86_64
    return _mm_cvtsi128_si64(state0);
#  else
    return _mm_cvtsi128_si32(state0);
#  endif
}

uint qHash(const AesHashString &str, uint seed)
{
    return aeshash(reinterpret_cast<const uchar *>(str.constData()), str.size() * sizeof(QChar), seed);
}
#endif

namespace Allan {
// We assume the definitions below are recognized and replaced
// by rotate instruction by compilers.
static inline uint64_t qt_rotate_left(uint64_t v, int count)
{
    return (v << count) | (v >> (64 -count));
}

static inline uint64_t qt_rotate_right(uint64_t v, int count)
{
    return (v << (64 - count)) | (v >> count);
}

static inline uint32_t qt_rotate_left(uint32_t v, int count)
{
    return (v << count) | (v >> (32 -count));
}

static inline uint32_t qt_rotate_right(uint32_t v, int count)
{
    return (v << (32 - count)) | (v >> count);
}

struct SipState {
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
};

static inline SipState sipcompress(SipState state)
{
    state.s0 += state.s1;
    state.s2 += state.s3;
    state.s1 = qt_rotate_left(state.s1, 13);
    state.s3 = qt_rotate_left(state.s3, 16);
    state.s1 ^= state.s0;
    state.s3 ^= state.s2;
    state.s0 = qt_rotate_left(state.s0, 32);
    state.s2 += state.s1;
    state.s0 += state.s3;
    state.s1 = qt_rotate_left(state.s1, 17);
    state.s3 = qt_rotate_left(state.s3, 21);
    state.s1 ^= state.s2;
    state.s3 ^= state.s0;
    state.s2 = qt_rotate_left(state.s2, 32);
    return state;
}

static inline SipState sipstep(const uint64_t val, SipState state)
{
    state.s3 ^= val;
    state = sipcompress(state);
    state = sipcompress(state);
    state.s0 ^= val;
    return state;
}

static inline SipState sipfinal(SipState state)
{
    state.s2 ^= 0xff;
    state = sipcompress(state);
    state = sipcompress(state);
    state = sipcompress(state);
    state = sipcompress(state);
    return state;
}

static uint siphash(const uchar *p, int len, uint seed) Q_DECL_NOTHROW
{
    Key128 k;
    makeKey128(k, len, seed);
    uint64_t key0, key1;
    memcpy(&key0, reinterpret_cast<uchar *>(&k), sizeof(quint64));
    memcpy(&key1, reinterpret_cast<uchar *>(&k) + 8, sizeof(quint64));

    SipState state;
    state.s0 = key0 ^ Q_UINT64_C(0x736f6d6570736575);
    state.s1 = key1 ^ Q_UINT64_C(0x646f72616e646f6d);
    state.s2 = key0 ^ Q_UINT64_C(0x6c7967656e657261);
    state.s3 = key1 ^ Q_UINT64_C(0x7465646279746573);

    int i = 0;
    for (; i < (len - 7); i += 8) {
        const uint64_t val = qFromUnaligned<uint64_t>(p + i);
        state = sipstep(val, state);
    }

    uint64_t val = 0;
    memcpy(&val, p + i, len - i);
    state = sipstep(val, state);

    state = sipfinal(state);

    return state.s0 * state.s1 * state.s2 * state.s3;
}
}

uint qHash(const AllansSipHashString &str, uint seed)
{
    return Allan::siphash(reinterpret_cast<const uchar *>(str.constData()), str.size() * sizeof(QChar), seed);
}

static int siphash(uint8_t *out, const uint8_t *in, uint64_t inlen, const uint8_t *k);
uint qHash(const SipHashString &str, uint seed)
{
    uint8_t out[sizeof(quint64)] = { };

    if (str.isEmpty()) {
        // we've done this in the past, so we keep this behavior
        return seed;
    }
    // and takes 128-bit keys
    Key128 k;
    makeKey128(k, str.size() * sizeof(QChar), seed);
    siphash(out, reinterpret_cast<const uchar *>(str.constData()), str.size() * sizeof(QChar), reinterpret_cast<uchar *>(&k));
    return qFromUnaligned<uint>(out);
}

QT_END_NAMESPACE

#include "../../../../../src/3rdparty/siphash/siphash24.c"
