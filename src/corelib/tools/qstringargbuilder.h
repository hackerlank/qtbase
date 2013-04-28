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

#ifndef QSTRINGARGBUILDER_H
#define QSTRINGARGBUILDER_H

#include <QtCore/qstring.h>

QT_BEGIN_NAMESPACE

class QStringArgBuilder : public QString
{
    friend class QString;
public:
    QStringArgBuilder(const QString &other)
        : QString(other)
    {}
    QStringArgBuilder(const QStringArgBuilder &other)
        : QString(other)
    {}

#ifdef Q_COMPILER_RVALUE_REFS
    QStringArgBuilder(QString &&other)
        : QString(std::move(other))
    {}
    QStringArgBuilder(QStringArgBuilder &&other)
        : QString(std::move(other))
    {}
#endif

    QStringArgBuilder &arg(qlonglong a, int fieldWidth=0, int base=10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(a, fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(qulonglong a, int fieldWidth=0, int base=10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(a, fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(long a, int fieldWidth=0, int base=10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(qlonglong(a), fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(ulong a, int fieldWidth=0, int base=10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(qulonglong(a), fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(int a, int fieldWidth = 0, int base = 10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(qlonglong(a), fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(uint a, int fieldWidth = 0, int base = 10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(qulonglong(a), fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(short a, int fieldWidth = 0, int base = 10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(qlonglong(a), fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(ushort a, int fieldWidth = 0, int base = 10,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(qulonglong(a), fieldWidth, base, fillChar); return *this; }
    QStringArgBuilder &arg(double a, int fieldWidth = 0, char fmt = 'g', int prec = -1,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(a, fieldWidth, fmt, prec, fillChar); return *this; }
    QStringArgBuilder &arg(char a, int fieldWidth = 0,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(QLatin1Char(a), fieldWidth, fillChar); return *this; }
    QStringArgBuilder &arg(QChar a, int fieldWidth = 0,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(a, fieldWidth, fillChar); return *this; }
    QStringArgBuilder &arg(const QString &a, int fieldWidth = 0,
                           QChar fillChar = QLatin1Char(' ')) Q_REQUIRED_RESULT
    { applyArg(a, fieldWidth, fillChar); return *this; }

#if defined(Q_COMPILER_VARIADIC_TEMPLATES) && defined(Q_COMPILER_RVALUE_REFS)
    template <typename... Args>
    Q_REQUIRED_RESULT QStringArgBuilder &arg(const QString &a1, const QString &a2, const Args &... args)
    {
        applyMultiArg(a1, a2, QString(args)...);
        return *this;
    }

private:
    template <typename... Args>
    void applyMultiArg(const QString &a1, const QString &a2, const Args &... args)
    {
        const QString *const v[] = { &a1, &a2, &args... };
        applyMultiArg(2 + sizeof...(args), v);
    }
#endif

private:
    Q_CORE_EXPORT void applyArg(qlonglong a, int fieldWidth, int base, QChar fillChar);
    Q_CORE_EXPORT void applyArg(qulonglong a, int fieldWidth, int base, QChar fillChar);
    Q_CORE_EXPORT void applyArg(double a, int fieldWidth, char fmt, int prec, QChar fillChar);
    Q_CORE_EXPORT void applyArg(QChar a, int fieldWidth, QChar fillChar);
    Q_CORE_EXPORT void applyArg(const QString &a, int fieldWidth, QChar fillChar);
    Q_CORE_EXPORT void applyMultiArg(unsigned count, const QString * const *);
};

#ifdef QT_QSTRING_ARG_NEW
#  ifdef Q_COMPILER_REF_QUALIFIERS
template <typename... Args>
QStringArgBuilder QString::arg(Args &&... args) const &
{
    QStringArgBuilder builder(*this);
    QStringArgBuilder &retval = builder.arg(std::forward<Args>(args)...);
    Q_UNUSED(retval);
    return builder;
}

template <typename... Args>
QStringArgBuilder QString::arg(Args &&... args) &&
{
    QStringArgBuilder builder(std::move(*this));
    QStringArgBuilder &retval = builder.arg(std::forward<Args>(args)...);
    Q_UNUSED(retval);
    return builder;
}
#  else
template <typename... Args>
QStringArgBuilder QString::arg(Args &&... args) const
{
    QStringArgBuilder builder(*this);
    QStringArgBuilder &retval = builder.arg(std::forward<Args>(args)...);
    Q_UNUSED(retval);
    return builder;
}
#  endif
#endif

QT_END_NAMESPACE

#endif // QSTRINGARGBUILDER_H
