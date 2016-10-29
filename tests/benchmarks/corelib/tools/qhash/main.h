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

#include <QString>

struct Qt4String : QString
{
    Qt4String() {}
    Qt4String(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const Qt4String &);
QT_END_NAMESPACE

struct Qt50String : QString
{
    Qt50String() {}
    Qt50String(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const Qt50String &, uint seed = 0);
QT_END_NAMESPACE


struct JavaString : QString
{
    JavaString() {}
    JavaString(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const JavaString &);
QT_END_NAMESPACE

struct Crc32String : QString
{
    Crc32String() {}
    Crc32String(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const Crc32String &, uint seed = 0);
QT_END_NAMESPACE

struct SipHashString : QString
{
    SipHashString() = default;
    SipHashString(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const SipHashString &, uint seed = 0);
QT_END_NAMESPACE

struct AllansSipHashString : QString
{
    AllansSipHashString() = default;
    AllansSipHashString(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const AllansSipHashString &, uint seed = 0);
QT_END_NAMESPACE

struct AesHashString : QString
{
    AesHashString() = default;
    AesHashString(const QString &s) : QString(s) {}
};

QT_BEGIN_NAMESPACE
uint qHash(const AesHashString &, uint seed = 0);
QT_END_NAMESPACE
