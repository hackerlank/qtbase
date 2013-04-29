/****************************************************************************
**
** Copyright (C) 2016 The Qt Company.
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

#if defined(QSTRING_H) && !defined(QT_BOOTSTRAPPED)
#  error "This file cannot be compiled with pre-compiled headers"
//       except for configure.exe
#endif
#define QT_COMPILING_QSTRINGARGBUILDER_CPP

#include "qstringargbuilder.h"
#include <qdebug.h>
#include "qlocale.h"
#include "qlocale_p.h"
#include "qvarlengtharray.h"

#include <limits.h>
#include <stdarg.h>

QT_BEGIN_NAMESPACE

inline bool qIsUpper(char ch)
{
    return ch >= 'A' && ch <= 'Z';
}

inline char qToLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 'a';
    else
        return ch;
}


struct ArgEscapeData
{
    int min_escape;            // lowest escape sequence number
    int occurrences;           // number of occurrences of the lowest escape sequence number
    int locale_occurrences;    // number of occurrences of the lowest escape sequence number that
                               // contain 'L'
    int escape_len;            // total length of escape sequences which will be replaced
};

static ArgEscapeData findArgEscapes(const QString &s)
{
    const QChar *uc_begin = s.unicode();
    const QChar *uc_end = uc_begin + s.length();

    ArgEscapeData d;

    d.min_escape = INT_MAX;
    d.occurrences = 0;
    d.escape_len = 0;
    d.locale_occurrences = 0;

    const QChar *c = uc_begin;
    while (c != uc_end) {
        while (c != uc_end && c->unicode() != '%')
            ++c;

        if (c == uc_end)
            break;
        const QChar *escape_start = c;
        if (++c == uc_end)
            break;

        bool locale_arg = false;
        if (c->unicode() == 'L') {
            locale_arg = true;
            if (++c == uc_end)
                break;
        }

        int escape = c->digitValue();
        if (escape == -1)
            continue;

        ++c;

        if (c != uc_end) {
            int next_escape = c->digitValue();
            if (next_escape != -1) {
                escape = (10 * escape) + next_escape;
                ++c;
            }
        }

        if (escape > d.min_escape)
            continue;

        if (escape < d.min_escape) {
            d.min_escape = escape;
            d.occurrences = 0;
            d.escape_len = 0;
            d.locale_occurrences = 0;
        }

        ++d.occurrences;
        if (locale_arg)
            ++d.locale_occurrences;
        d.escape_len += c - escape_start;
    }
    return d;
}

static QString replaceArgEscapes(const QString &s, const ArgEscapeData &d, int field_width,
                                 const QString &arg, const QString &larg, QChar fillChar = QLatin1Char(' '))
{
    const QChar *uc_begin = s.unicode();
    const QChar *uc_end = uc_begin + s.length();

    int abs_field_width = qAbs(field_width);
    int result_len = s.length()
                     - d.escape_len
                     + (d.occurrences - d.locale_occurrences)
                     *qMax(abs_field_width, arg.length())
                     + d.locale_occurrences
                     *qMax(abs_field_width, larg.length());

    QString result(result_len, Qt::Uninitialized);
    QChar *result_buff = (QChar*) result.data();

    QChar *rc = result_buff;
    const QChar *c = uc_begin;
    int repl_cnt = 0;
    while (c != uc_end) {
        /* We don't have to check if we run off the end of the string with c,
           because as long as d.occurrences > 0 we KNOW there are valid escape
           sequences. */

        const QChar *text_start = c;

        while (c->unicode() != '%')
            ++c;

        const QChar *escape_start = c++;

        bool locale_arg = false;
        if (c->unicode() == 'L') {
            locale_arg = true;
            ++c;
        }

        int escape = c->digitValue();
        if (escape != -1) {
            if (c + 1 != uc_end && (c + 1)->digitValue() != -1) {
                escape = (10 * escape) + (c + 1)->digitValue();
                ++c;
            }
        }

        if (escape != d.min_escape) {
            memcpy(rc, text_start, (c - text_start)*sizeof(QChar));
            rc += c - text_start;
        }
        else {
            ++c;

            memcpy(rc, text_start, (escape_start - text_start)*sizeof(QChar));
            rc += escape_start - text_start;

            uint pad_chars;
            if (locale_arg)
                pad_chars = qMax(abs_field_width, larg.length()) - larg.length();
            else
                pad_chars = qMax(abs_field_width, arg.length()) - arg.length();

            if (field_width > 0) { // left padded
                for (uint i = 0; i < pad_chars; ++i)
                    (rc++)->unicode() = fillChar.unicode();
            }

            if (locale_arg) {
                memcpy(rc, larg.unicode(), larg.length()*sizeof(QChar));
                rc += larg.length();
            }
            else {
                memcpy(rc, arg.unicode(), arg.length()*sizeof(QChar));
                rc += arg.length();
            }

            if (field_width < 0) { // right padded
                for (uint i = 0; i < pad_chars; ++i)
                    (rc++)->unicode() = fillChar.unicode();
            }

            if (++repl_cnt == d.occurrences) {
                memcpy(rc, c, (uc_end - c)*sizeof(QChar));
                rc += uc_end - c;
                Q_ASSERT(rc - result_buff == result_len);
                c = uc_end;
            }
        }
    }
    Q_ASSERT(rc == result_buff + result_len);

    return result;
}

static void replaceArg(QString &pattern, const QString &a, int fieldWidth, QChar fillChar)
{
    ArgEscapeData d = findArgEscapes(pattern);

    if (d.occurrences == 0) {
        qWarning("QString::arg: Argument missing: %s, %s", pattern.toLocal8Bit().data(),
                  a.toLocal8Bit().data());
        return;
    }
    pattern = replaceArgEscapes(pattern, d, fieldWidth, a, a, fillChar);
}

static void replaceArg(QString &pattern, qlonglong a, int fieldWidth, int base, QChar fillChar)
{
    ArgEscapeData d = findArgEscapes(pattern);

    if (d.occurrences == 0) {
        qWarning() << "QString::arg: Argument missing:" << pattern << ',' << a;
        return;
    }

    unsigned flags = QLocaleData::NoFlags;
    if (fillChar == QLatin1Char('0'))
        flags = QLocaleData::ZeroPadded;

    QString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = QLocaleData::c()->longLongToString(a, -1, base, fieldWidth, flags);

    QString locale_arg;
    if (d.locale_occurrences > 0) {
        QLocale locale;
        if (!(locale.numberOptions() & QLocale::OmitGroupSeparator))
            flags |= QLocaleData::ThousandsGroup;
        locale_arg = QLocaleData::defaultData()->longLongToString(a, -1, base, fieldWidth, flags);
    }

    pattern = replaceArgEscapes(pattern, d, fieldWidth, arg, locale_arg, fillChar);
}

static void replaceArg(QString &pattern, qulonglong a, int fieldWidth, int base, QChar fillChar)
{
    ArgEscapeData d = findArgEscapes(pattern);

    if (d.occurrences == 0) {
        qWarning() << "QString::arg: Argument missing:" << pattern << ',' << a;
        return;
    }

    unsigned flags = QLocaleData::NoFlags;
    if (fillChar == QLatin1Char('0'))
        flags = QLocaleData::ZeroPadded;

    QString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = QLocaleData::c()->unsLongLongToString(a, -1, base, fieldWidth, flags);

    QString locale_arg;
    if (d.locale_occurrences > 0) {
        QLocale locale;
        if (!(locale.numberOptions() & QLocale::OmitGroupSeparator))
            flags |= QLocaleData::ThousandsGroup;
        locale_arg = QLocaleData::defaultData()->longLongToString(a, -1, base, fieldWidth, flags);
    }

    pattern = replaceArgEscapes(pattern, d, fieldWidth, arg, locale_arg, fillChar);
}

static void replaceArg(QString &pattern, double a, int fieldWidth, char fmt, int prec, QChar fillChar)
{
    ArgEscapeData d = findArgEscapes(pattern);

    if (d.occurrences == 0) {
        qWarning("QString::arg: Argument missing: %s, %g", pattern.toLocal8Bit().data(), a);
        return;
    }

    unsigned flags = QLocaleData::NoFlags;
    if (fillChar == QLatin1Char('0'))
        flags = QLocaleData::ZeroPadded;

    if (qIsUpper(fmt))
        flags |= QLocaleData::CapitalEorX;
    fmt = qToLower(fmt);

    QLocaleData::DoubleForm form = QLocaleData::DFDecimal;
    switch (fmt) {
    case 'f':
        form = QLocaleData::DFDecimal;
        break;
    case 'e':
        form = QLocaleData::DFExponent;
        break;
    case 'g':
        form = QLocaleData::DFSignificantDigits;
        break;
    default:
#if defined(QT_CHECK_RANGE)
        qWarning("QString::arg: Invalid format char '%c'", fmt);
#endif
        break;
    }

    QString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = QLocaleData::c()->doubleToString(a, prec, form, fieldWidth, flags);

    QString locale_arg;
    if (d.locale_occurrences > 0) {
        QLocale locale;
        if (!(locale.numberOptions() & QLocale::OmitGroupSeparator))
            flags |= QLocaleData::ThousandsGroup;
        if (!(locale.numberOptions() & QLocale::OmitLeadingZeroInExponent))
            flags |= QLocaleData::ZeroPadExponent;
        locale_arg = QLocaleData::defaultData()->doubleToString(a, prec, form, fieldWidth, flags);
    }

    pattern = replaceArgEscapes(pattern, d, fieldWidth, arg, locale_arg, fillChar);
}

static int getEscape(const QChar *uc, int *pos, int len, int maxNumber = 999)
{
    int i = *pos;
    ++i;
    if (i < len && uc[i] == QLatin1Char('L'))
        ++i;
    if (i < len) {
        int escape = uc[i].unicode() - '0';
        if (uint(escape) >= 10U)
            return -1;
        ++i;
        while (i < len) {
            int digit = uc[i].unicode() - '0';
            if (uint(digit) >= 10U)
                break;
            escape = (escape * 10) + digit;
            ++i;
        }
        if (escape <= maxNumber) {
            *pos = i;
            return escape;
        }
    }
    return -1;
}

#ifndef QT_QSTRING_ARG_NEW
// this can be defined if we're in a bootstrapped build

/*!
  Returns a copy of this string with the lowest numbered place marker
  replaced by string \a a, i.e., \c %1, \c %2, ..., \c %99.

  \a fieldWidth specifies the minimum amount of space that argument \a
  a shall occupy. If \a a requires less space than \a fieldWidth, it
  is padded to \a fieldWidth with character \a fillChar.  A positive
  \a fieldWidth produces right-aligned text. A negative \a fieldWidth
  produces left-aligned text.

  This example shows how we might create a \c status string for
  reporting progress while processing a list of files:

  \snippet qstring/main.cpp 11

  First, \c arg(i) replaces \c %1. Then \c arg(total) replaces \c
  %2. Finally, \c arg(fileName) replaces \c %3.

  One advantage of using arg() over asprintf() is that the order of the
  numbered place markers can change, if the application's strings are
  translated into other languages, but each arg() will still replace
  the lowest numbered unreplaced place marker, no matter where it
  appears. Also, if place marker \c %i appears more than once in the
  string, the arg() replaces all of them.

  If there is no unreplaced place marker remaining, a warning message
  is output and the result is undefined. Place marker numbers must be
  in the range 1 to 99.
*/
QString QString::arg(const QString &a, int fieldWidth, QChar fillChar) const
{
    QString retval(*this);
    replaceArg(retval, a, fieldWidth, fillChar);
    return retval;
}

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2) const
  \overload arg()

  This is the same as \c {str.arg(a1).arg(a2)}, except that the
  strings \a a1 and \a a2 are replaced in one pass. This can make a
  difference if \a a1 contains e.g. \c{%1}:

  \snippet qstring/main.cpp 13

  A similar problem occurs when the numbered place markers are not
  white space separated:

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 97

  Let's look at the substitutions:
  \list
  \li First, \c Hello replaces \c {%1} so the string becomes \c {"Hello%3%2"}.
  \li Then, \c 20 replaces \c {%2} so the string becomes \c {"Hello%320"}.
  \li Since the maximum numbered place marker value is 99, \c 50 replaces \c {%32}.
  \endlist
  Thus the string finally becomes \c {"Hello500"}.

  In such cases, the following yields the expected results:

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 98
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3) const
  \overload arg()

  This is the same as calling \c str.arg(a1).arg(a2).arg(a3), except
  that the strings \a a1, \a a2 and \a a3 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4)}, except that the strings \a
  a1, \a a2, \a a3 and \a a4 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5)}, except that the strings
  \a a1, \a a2, \a a3, \a a4, and \a a5 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6))}, except that
  the strings \a a1, \a a2, \a a3, \a a4, \a a5, and \a a6 are
  replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6, const QString& a7) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6,
  and \a a7 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6, const QString& a7, const QString& a8) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6, \a
  a7, and \a a8 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6, const QString& a7, const QString& a8, const QString& a9) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8).arg(a9)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6, \a
  a7, \a a8, and \a a9 are replaced in one pass.
*/

/*! \fn QString QString::arg(int a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  The \a a argument is expressed in base \a base, which is 10 by
  default and must be between 2 and 36. For bases other than 10, \a a
  is treated as an unsigned integer.

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale, set by QLocale::setDefault(). If no default
  locale was specified, the "C" locale is used. The 'L' flag is
  ignored if \a base is not 10.

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 14

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn QString QString::arg(uint a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn QString QString::arg(long a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a a argument is expressed in the given \a base, which is 10 by
  default and must be between 2 and 36.

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale. The default locale is determined from the
  system's locale settings at application startup. It can be changed
  using QLocale::setDefault(). The 'L' flag is ignored if \a base is
  not 10.

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 14

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn QString QString::arg(ulong a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a to a string. The base must be between 2 and 36, with 8
  giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/
QString QString::arg(qlonglong a, int fieldWidth, int base, QChar fillChar) const
{
    QString retval(*this);
    replaceArg(retval, a, fieldWidth, base, fillChar);
    return retval;
}

/*!
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. \a base must be between 2 and 36, with 8
  giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/
QString QString::arg(qulonglong a, int fieldWidth, int base, QChar fillChar) const
{
    QString retval(*this);
    replaceArg(retval, a, fieldWidth, base, fillChar);
    return retval;
}

/*!
  \overload arg()

  \fn QString QString::arg(short a, int fieldWidth, int base, QChar fillChar) const

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
  \fn QString QString::arg(ushort a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
    \overload arg()
*/
QString QString::arg(QChar a, int fieldWidth, QChar fillChar) const
{
    QString c;
    c += a;
    return arg(c, fieldWidth, fillChar);
}

/*!
  \overload arg()

  The \a a argument is interpreted as a Latin-1 character.
*/
QString QString::arg(char a, int fieldWidth, QChar fillChar) const
{
    QString c;
    c += QLatin1Char(a);
    return arg(c, fieldWidth, fillChar);
}

/*!
  \fn QString QString::arg(double a, int fieldWidth, char format, int precision, QChar fillChar) const
  \overload arg()

  Argument \a a is formatted according to the specified \a format and
  \a precision. See \l{Argument Formats} for details.

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar.  A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  \snippet code/src_corelib_tools_qstring.cpp 2

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale, set by QLocale::setDefault(). If no
  default locale was specified, the "C" locale is used.

  If \a fillChar is '0' (the number 0, ASCII 48), this function will
  use the locale's zero to pad. For negative numbers, the zero padding
  will probably appear before the minus sign.

  \sa QLocale::toString()
*/
QString QString::arg(double a, int fieldWidth, char fmt, int prec, QChar fillChar) const
{
    QString retval(*this);
    replaceArg(retval, a, fieldWidth, fmt, prec, fillChar);
    return retval;
}
#endif // QT_QSTRING_ARG_NEW

/*
    Algorithm for multiArg:

    1. Parse the string as a sequence of verbatim text and placeholders (%L?\d{,3}).
       The L is parsed and accepted for compatibility with non-multi-arg, but since
       multiArg only accepts strings as replacements, the localization request can
       be safely ignored.
    2. The result of step (1) is a list of (string-ref,int)-tuples. The string-ref
       either points at text to be copied verbatim (in which case the int is -1),
       or, initially, at the textual representation of the placeholder. In that case,
       the int contains the numerical number as parsed from the placeholder.
    3. Next, collect all the non-negative ints found, sort them in ascending order and
       remove duplicates.
       3a. If the result has more entires than multiArg() was given replacement strings,
           we have found placeholders we can't satisfy with replacement strings. That is
           fine (there could be another .arg() call coming after this one), so just
           truncate the result to the number of actual multiArg() replacement strings.
       3b. If the result has less entries than multiArg() was given replacement strings,
           the string is missing placeholders. This is an error that the user should be
           warned about.
    4. The result of step (3) is a mapping from the index of any replacement string to
       placeholder number. This is the wrong way around, but since placeholder
       numbers could get as large as 999, while we typically don't have more than 9
       replacement strings, we trade 4K of sparsely-used memory for doing a reverse lookup
       each time we need to map a placeholder number to a replacement string index
       (that's a linear search; but still *much* faster than using an associative container).
    5. Next, for each of the tuples found in step (1), do the following:
       5a. If the int is negative, do nothing.
       5b. Otherwise, if the int is found in the result of step (3) at index I, replace
           the string-ref with a string-ref for the (complete) I'th replacement string.
       5c. Otherwise, do nothing.
    6. Concatenate all string refs into a single result string.
*/

namespace {
struct Part
{
    Part() : stringRef(), number(0) {}
    Part(const QString &s, int pos, int len, int num = -1) Q_DECL_NOTHROW
        : stringRef(&s, pos, len), number(num) {}

    QStringRef stringRef;
    int number;
};
} // unnamed namespace

template <>
class QTypeInfo<Part> : public QTypeInfoMerger<Part, QStringRef, int> {}; // Q_DECLARE_METATYPE


namespace {

enum { ExpectedParts = 32 };

typedef QVarLengthArray<Part, ExpectedParts> ParseResult;
typedef QVarLengthArray<int, ExpectedParts/2> ArgIndexToPlaceholderMap;

static ParseResult parseMultiArgFormatString(const QString &s)
{
    ParseResult result;

    const QChar *uc = s.constData();
    const int len = s.size();
    const int end = len - 1;
    int i = 0;
    int last = 0;

    while (i < end) {
        if (uc[i] == QLatin1Char('%')) {
            int percent = i;
            int number = getEscape(uc, &i, len);
            if (number != -1) {
                if (last != percent)
                    result.push_back(Part(s, last, percent - last)); // literal text (incl. failed placeholders)
                result.push_back(Part(s, percent, i - percent, number));  // parsed placeholder
                last = i;
                continue;
            }
        }
        ++i;
    }

    if (last < len)
        result.push_back(Part(s, last, len - last)); // trailing literal text

    return result;
}

static ArgIndexToPlaceholderMap makeArgIndexToPlaceholderMap(const ParseResult &parts)
{
    ArgIndexToPlaceholderMap result;

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        if (it->number >= 0)
            result.push_back(it->number);
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()),
                 result.end());

    return result;
}

static int resolveStringRefsAndReturnTotalSize(ParseResult &parts, const ArgIndexToPlaceholderMap &argIndexToPlaceholderMap, const QString *const args[])
{
    int totalSize = 0;
    for (ParseResult::iterator pit = parts.begin(), end = parts.end(); pit != end; ++pit) {
        if (pit->number != -1) {
            const ArgIndexToPlaceholderMap::const_iterator ait
                    = std::find(argIndexToPlaceholderMap.begin(), argIndexToPlaceholderMap.end(), pit->number);
            if (ait != argIndexToPlaceholderMap.end())
                pit->stringRef = QStringRef(args[ait - argIndexToPlaceholderMap.begin()]);
        }
        totalSize += pit->stringRef.size();
    }
    return totalSize;
}

static QString replaceMultiArg(const QString &pattern, int numArgs, const QString *const *args)
{
    // Step 1-2 above
    ParseResult parts = parseMultiArgFormatString(pattern);

    // 3-4
    ArgIndexToPlaceholderMap argIndexToPlaceholderMap = makeArgIndexToPlaceholderMap(parts);

    if (argIndexToPlaceholderMap.size() > numArgs) // 3a
        argIndexToPlaceholderMap.resize(numArgs);
    else if (argIndexToPlaceholderMap.size() < numArgs) // 3b
        qWarning("QString::arg: %d argument(s) missing in %s",
                 numArgs - argIndexToPlaceholderMap.size(), pattern.toLocal8Bit().data());

    // 5
    const int totalSize = resolveStringRefsAndReturnTotalSize(parts, argIndexToPlaceholderMap, args);

    // 6:
    QString result(totalSize, Qt::Uninitialized);
    QChar *out = result.data();

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        if (const int sz = it->stringRef.size()) {
            memcpy(out, it->stringRef.constData(), sz * sizeof(QChar));
            out += sz;
        }
    }

    return result;
}

} // unnamed namespace

#ifndef QT_QSTRING_ARG_NEW
QString QString::multiArg(int numArgs, const QString **args) const
{
    return replaceMultiArg(*this, numArgs, args);
}
#endif // QT_QSTRING_ARG_NEW

void QStringArgBuilder::applyArg(qlonglong a, int fieldWidth, int base, QChar fillChar)
{
    replaceArg(*this, a, fieldWidth, base, fillChar);
}

void QStringArgBuilder::applyArg(qulonglong a, int fieldWidth, int base, QChar fillChar)
{
    replaceArg(*this, a, fieldWidth, base, fillChar);
}

void QStringArgBuilder::applyArg(double a, int fieldWidth, char fmt, int prec, QChar fillChar)
{
    replaceArg(*this, a, fieldWidth, fmt, prec, fillChar);
}

void QStringArgBuilder::applyArg(QChar a, int fieldWidth, QChar fillChar)
{
    QString s(1, Qt::Uninitialized);
    s[0] = a;
    applyArg(s, fieldWidth, fillChar);
}

void QStringArgBuilder::applyArg(const QString &a, int fieldWidth, QChar fillChar)
{
    replaceArg(*this, a, fieldWidth, fillChar);
}

void QStringArgBuilder::applyMultiArg(unsigned count, const QString * const *args)
{
    QString::operator =(replaceMultiArg(*this, count, args));
}

QT_END_NAMESPACE
