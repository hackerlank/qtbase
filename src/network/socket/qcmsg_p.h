/****************************************************************************
**
** Copyright (C) 2016 Intel Corporation.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#ifndef QCMSG_P_H
#define QCMSG_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QNativeSocketEngine class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qnativesocketengine_p.h"

#ifdef Q_OS_WIN
#  include <winsock2.h>
#  undef CMSG_DATA      // wincrypt.h, unrelated
#  define CMSG_DATA     WSA_CMSG_DATA
#  define CMSG_FIRSTHDR WSA_CMSG_FIRSTHDR
#  define CMSG_LEN      WSA_CMSG_LEN
#  define CMSG_NXTHDR   WSA_CMSG_NXTHDR
#  define CMSG_SPACE    WSA_CMSG_SPACE
typedef WSACMSGHDR cmsghdr;
typedef WSAMSG msghdr;
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  if defined(Q_OS_BSD4) || defined(Q_OS_QNX)
#    include <net/if_dl.h>
#  endif
#  ifndef QT_NO_SCTP
#    include <netinet/sctp.h>
#  endif
#endif

#ifdef SOL_IP
Q_STATIC_ASSERT(SOL_IP == IPPROTO_IP);
#endif
#ifdef SOL_IPV6
Q_STATIC_ASSERT(SOL_IPV6 == IPPROTO_IPV6);
#endif

QT_BEGIN_NAMESPACE

namespace {
// wrappers for plain int payloads
struct in_ttl { int value; };
struct in6_hoplimit { int value; };

template <typename> struct AncillaryDataTags;

template <> struct AncillaryDataTags<in_ttl>
{ static const int level = IPPROTO_IP, type = IP_TTL; };

#ifdef IP_PKTINFO       // Linux, Darwin (macOS), NetBSD, Solaris, Windows
template <> struct AncillaryDataTags<in_pktinfo>
{ static const int level = IPPROTO_IP, type = IP_PKTINFO; };
#else
#  if defined(IP_RECVDSTADDR)    // FreeBSD, OpenBSD and QNX
#    if defined(IP_SENDSRCADDR)    // FreeBSD and OpenBSD
// these two constants must be the same if they both exist
Q_STATIC_ASSERT(IP_RECVDSTADDR == IP_SENDSRCADDR);
#    endif
struct in_pktinfo { struct in_addr ipi_addr; };
template <> struct AncillaryDataTags<in_pktinfo>
{ static const int level = IPPROTO_IP, type = IP_RECVDSTADDR; };
#  endif
#  if defined(IP_RECVIF) // FreeBSD and QNX
template <> struct AncillaryDataTags<sockaddr_dl>
{ static const int level = IPPROTO_IP, type = IP_RECVIF; };
#  endif
#endif

#ifndef QT_NO_SCTP
template <> struct AncillaryDataTags<sctp_sndrcvinfo>
{ static const int level = IPPROTO_SCTP, type = SCTP_SNDRCV; };
#endif

template <> struct AncillaryDataTags<in6_hoplimit>
{ static const int level = IPPROTO_IPV6, type = IPV6_HOPLIMIT; };
template <> struct AncillaryDataTags<in6_pktinfo>
{ static const int level = IPPROTO_IPV6, type = IPV6_PKTINFO; };

struct AncillaryData : public cmsghdr
{
    // keep POD, so don't declare any constructors or destructors
    template <typename T> bool isType() const
    {
        return cmsg_level == AncillaryDataTags<T>::level && cmsg_type == AncillaryDataTags<T>::type;
    }

    template <typename T> T *payload()
    {
        if (!isType<T>() || cmsg_len < CMSG_LEN(sizeof(T)))
            return nullptr;
        return reinterpret_cast<T *>(CMSG_DATA(this));
    }
};

template <typename... Types> struct SocketMessageBuffer;
template <> struct SocketMessageBuffer<>
{
    // termination
    static size_t requiredSpace() { return 0; }
};

template <typename First, typename... Types>
struct SocketMessageBuffer<First, Types...> : public SocketMessageBuffer<Types...>
{
private:
    Q_STATIC_ASSERT(sizeof(AncillaryDataTags<First>::level));
    Q_STATIC_ASSERT(sizeof(AncillaryDataTags<First>::type));

    // Note: never use the actual TypedPayload member in the union below.
    // The order from the kernel may not match the order of the template
    // parameters we used.
    struct TypedPayload {
        cmsghdr hdr;
        First data;
    };

    union Payload {
        // The payload size for a given ancillary data of type T is
        // CMSG_LEN(sizeof(T)), but due to alignment requirements, the distance
        // between the ancillary data of type T and the next ancillary data is
        // CMSG_SPACE(T). Normally, you'd expect CMSG_SPACE(T) ==
        // sizeof(TypedPayload), but that may be different if the OS kernel
        // runs on a different ABI than the userspace programs. So we need to
        // define a buffer instead.
        //
        // On some systems, the CMSG_SPACE macro isn't constexpr, so we can't
        // simply declare an array with the correct size. On those systems, we try
        // the TypedPayload structure and check for correct size at runtime, in
        // SocketMessageBuffer::commonInit(). (On NetBSD, the compiler emits 16
        // calls to __cmsg_alignbytes -- they really ought to mark the function
        // with __attribute__((const))).
        TypedPayload typed;

#if defined(Q_OS_QNX) || defined(Q_OS_NETBSD)
        uchar space[sizeof(TypedPayload)];
#else
        uchar space[CMSG_SPACE(sizeof(First))];
#endif

        static size_t requiredSpace() { return CMSG_SPACE(sizeof(First)); }
    };
    Payload data;

public:
    operator uchar *() { return data.space; }
    static size_t requiredSpace()
    { return Payload::requiredSpace() + SocketMessageBuffer<Types...>::requiredSpace(); }
};

template <typename... Types>
struct SocketMessage : private msghdr
{
#ifdef Q_OS_WIN
    // make WSAMSG members public
    using msghdr::Control;
    using msghdr::lpBuffers;
    using msghdr::dwBufferCount;
    using msghdr::name;
    using msghdr::namelen;
    using msghdr::dwFlags;

private:
    WSABUF buf;
    void *&controlBuffer() { return reinterpret_cast<void *&>(Control.buf); }
    DWORD &controlLength() { return Control.len; }
#else
    // make struct msghdr members public
    using msghdr::msg_control;
    using msghdr::msg_controllen;
    using msghdr::msg_name;
    using msghdr::msg_namelen;
    using msghdr::msg_iov;
    using msghdr::msg_iovlen;
    using msghdr::msg_flags;

private:
    struct iovec vec;
    void *&controlBuffer() { return msg_control; }
    decltype(msghdr::msg_controllen) &controlLength() { return msg_controllen; }
#endif

    union {
        SocketMessageBuffer<Types...> buffer;
        cmsghdr _for_alignment;
    };

    void commonInit(const char *data, qint64 len) Q_DECL_NOEXCEPT
    {
        // as noted above, CMSG_SPACE may not be constexpr, so we use Q_ASSERT
        Q_ASSERT(sizeof(buffer) >= buffer.requiredSpace());

        memset(static_cast<msghdr *>(this), 0, sizeof(msghdr));
        controlBuffer() = buffer;
#ifdef Q_OS_WIN
        name = &address.a;
        namelen = sizeof(address);
        lpBuffers = &buf;
        dwBufferCount = 1;
        buf.buf = const_cast<char *>(data);
        buf.len = len;
#else
        msg_name = &address.a;
        msg_namelen = sizeof(address);
        msg_iov = &vec;
        msg_iovlen = 1;
        vec.iov_base = const_cast<char *>(data);
        vec.iov_len = len;
#endif
    }

public:
    qt_sockaddr address;

    // sending API

    SocketMessage(const char *data, qint64 len)
    {
        commonInit(data, len);
    }

    msghdr *forSending()
    {
        if (controlLength() == 0) {
            // nothing was added, so pass a null pointer for the control buffer
            controlBuffer() = nullptr;
        }
        return this;
    }

    template <typename T> T *appendAncillaryPayload()
    {
        cmsghdr *next =
                reinterpret_cast<cmsghdr *>(static_cast<uchar *>(controlBuffer()) + controlLength());
        controlLength() += CMSG_SPACE(sizeof(T));
        Q_ASSERT(controlLength() <= buffer.requiredSpace());

        next->cmsg_len = CMSG_LEN(sizeof(T));
        next->cmsg_level = AncillaryDataTags<T>::level;
        next->cmsg_type = AncillaryDataTags<T>::type;
        return reinterpret_cast<T *>(CMSG_DATA(next));
    }


    // receiving API

    SocketMessage(char *data, qint64 maxLen)
    {
        commonInit(data, maxLen);
        controlLength() = sizeof(buffer);
    }

    msghdr *forReceiving()
    {
        return this;
    }

    AncillaryData *firstAncillaryPayload()
    {
        msghdr *hdr = this;
        return static_cast<AncillaryData *>(CMSG_FIRSTHDR(hdr));
    }

    AncillaryData *next(AncillaryData *cmsg)
    {
        msghdr *hdr = this;
        return static_cast<AncillaryData *>(CMSG_NXTHDR(hdr, cmsg));
    }
};

} // anonymous namespace
QT_END_NAMESPACE

#endif // QCMSG_P_H

