/****************************************************************************
**
** Copyright (C) 2016 Intel Corporation.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#include <QtTest>
#include <QScopedPointer>
#include <string.h>

class tst_ToString : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void basic();
    void strings();
    void pointers();
    void qtcore();
    void qtnetwork();
    void qtwidgets();
};

#define CHECK_TOSTRING(expr, expected)  \
    do { \
        QScopedArrayPointer<char> actual(QTest::toString(expr)); \
        QCOMPARE(actual.data(), expected); \
    } while (false)

#define CHECK_PTR_TOSTRING(ptr) \
    do { \
        char expected[3 + 2*sizeof(void*)]; \
        qsnprintf(expected, sizeof(expected), "%p", ptr); \
        CHECK_TOSTRING(ptr, expected); \
    } while (false)

void tst_ToString::basic()
{
    // integrals
    CHECK_TOSTRING(short(1), "1");
    CHECK_TOSTRING(ushort(1), "1");
    CHECK_TOSTRING(1, "1");
    CHECK_TOSTRING(1U, "1");
    CHECK_TOSTRING(1L, "1");
    CHECK_TOSTRING(1UL, "1");
    CHECK_TOSTRING(Q_INT64_C(1), "1");
    CHECK_TOSTRING(Q_UINT64_C(1), "1");
    CHECK_TOSTRING(Q_UINT64_C(~0), "18446744073709551615");

    // boolean prints 0 or 1
    CHECK_TOSTRING(false, "0");
    CHECK_TOSTRING(true, "1");

    // chars
    CHECK_TOSTRING('a', "'a'");
    CHECK_TOSTRING('\x80', "'\\x80'");
    CHECK_TOSTRING(uchar('a'), "97");
    CHECK_TOSTRING(uchar(128), "128");
    CHECK_TOSTRING((signed char)'a', "97");
    CHECK_TOSTRING((signed char)128, "-128");

    // floating point (long double not supported)
    CHECK_TOSTRING(0., "0");
    CHECK_TOSTRING(0.f, "0");
    CHECK_TOSTRING(qInf(), "inf");
    CHECK_TOSTRING(-qInf(), "-inf");
    CHECK_TOSTRING(qSNaN(), "nan");
}

void tst_ToString::strings()
{
    // plain strings are untransformed
    CHECK_TOSTRING("foo", "foo");

    // QByteArrays are quoted and printed with hex escapes
    CHECK_TOSTRING(QByteArray("\xc2\xa0""f"), "\"\\xC2\\xA0\"\"f\"");

    // QStrings and QLatin1Strings are quoted and printed with Unicode escapes
    CHECK_TOSTRING(QString("\xc2\xa0""f"), "\"\\u00A0f\"");
    CHECK_TOSTRING(QLatin1String("\xa0""f"), "\"\\u00A0f\"");

    // Unlike QDebug, QTest::toString will not decode surrogate pairs
    CHECK_TOSTRING(QString("\xf0\x90\x80\x80"), "\"\\uD800\\uDC00\"");
}

void testfunction(int) {}
void tst_ToString::pointers()
{
    // pointer to variables
    static int si = 0;
    int i;
    {
        int *p = 0;
        CHECK_PTR_TOSTRING(p);
        CHECK_PTR_TOSTRING(&i);
        CHECK_PTR_TOSTRING(&si);
    }
    {
        void *v = 0;
        CHECK_PTR_TOSTRING(v);
        CHECK_PTR_TOSTRING((void*)&i);
        CHECK_PTR_TOSTRING((void*)&si);
    }
    {
        const void *v = 0;
        CHECK_PTR_TOSTRING(v);
        CHECK_PTR_TOSTRING((const void*)&i);
        CHECK_PTR_TOSTRING((const void*)&si);
    }

    {
        void (*f)(int) = 0;
#ifdef Q_COMPILER_VARIADIC_TEMPLATES
        CHECK_PTR_TOSTRING(f);
        CHECK_PTR_TOSTRING(testfunction);
#else
        char *null = 0;
        CHECK_TOSTRING(f, null);
        CHECK_TOSTRING(testfunction, null);
#endif
    }
}

void tst_ToString::qtcore()
{
    CHECK_TOSTRING(QTime(), "Invalid QTime");
    CHECK_TOSTRING(QTime(0, 1, 2, 345), "00:01:02.345");

    CHECK_TOSTRING(QDate(), "Invalid QDate");
    CHECK_TOSTRING(QDate(2015, 11, 15), "2015/11/15");

    CHECK_TOSTRING(QDateTime(), "Invalid QDateTime");
    CHECK_TOSTRING(QDateTime(QDate(2015, 11, 15), QTime(17, 01, 0, 0), Qt::OffsetFromUTC, -7*3600),
                   "2015/11/15 17:01:00.000[UTC-07:00]");

    CHECK_TOSTRING(QChar('a'), "QChar: 'a' (0x61)");

    CHECK_TOSTRING(QPoint(0,0), "QPoint(0,0)");
    CHECK_TOSTRING(QPointF(0,0), "QPointF(0,0)");

    CHECK_TOSTRING(QSize(0,0), "QSize(0x0)");
    CHECK_TOSTRING(QSizeF(0,0), "QSizeF(0x0)");

    CHECK_TOSTRING(QRect(0, 0, 2, 2), "QRect(0,0 2x2) (bottomright 1,1)");
    CHECK_TOSTRING(QRectF(0, 0, 2, 2), "QRectF(0,0 2x2) (bottomright 2,2)");

    CHECK_TOSTRING(QUrl(), "Invalid URL: ");
    CHECK_TOSTRING(QUrl("foo/bar"), "foo/bar");
    CHECK_TOSTRING(QUrl("http://foo/bar/\xc2\xa0"), "http://foo/bar/%C2%A0");

    CHECK_TOSTRING(QVariant(), "QVariant()");
    CHECK_TOSTRING(QVariant(QVariant::Int), "QVariant(int)");
    CHECK_TOSTRING(QVariant(0), "QVariant(int,0)");
    CHECK_TOSTRING(QVariant(0u), "QVariant(uint,0)");
    CHECK_TOSTRING(QVariant(QString()), "QVariant(QString)");
    CHECK_TOSTRING(QVariant("hello"), "QVariant(QString,hello)");
    CHECK_TOSTRING(QVariant(QVariantHash()), "QVariant(QVariantHash,<value not representable as string>)");
}

void tst_ToString::qtnetwork()
{
#ifdef QT_NETWORK_LIB
    CHECK_TOSTRING(QHostAddress(), "<unknown address (parse error)>");
    CHECK_TOSTRING(QHostAddress(QHostAddress::Any), "QHostAddress::Any");
    CHECK_TOSTRING(QHostAddress("0.0.0.0"), "0.0.0.0");
    CHECK_TOSTRING(QHostAddress("::"), "::");
#else
    QSKIP("QtNetwork not enabled in this build");
#endif
}

void tst_ToString::qtwidgets()
{
#ifdef QT_WIDGETS_LIB
    CHECK_TOSTRING(QSizePolicy::Fixed, "Fixed");
    CHECK_TOSTRING(QSizePolicy::Ignored, "Ignored");
    CHECK_TOSTRING(QSizePolicy::DefaultType, "DefaultType");
    CHECK_TOSTRING(QSizePolicy(), "QSizePolicy(Fixed, Fixed, 0, 0, DefaultType, height for width: no, width for height: no, don't retain size when hidden)");
#else
    QSKIP("QtWidgets not enabled in this build");
#endif
}


QTEST_APPLESS_MAIN(tst_ToString)

#include "tst_tostring.moc"
