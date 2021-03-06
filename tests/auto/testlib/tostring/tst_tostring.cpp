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
#include <stdarg.h>

struct Type {};

class tst_ToString : public QObject
{
    Q_OBJECT
public:
    enum AnEnum { Value2 = 2, Value1 = 1 };
    Q_ENUM(AnEnum)

    enum Option { Option1 = 1, Option2 = 2 };
    Q_DECLARE_FLAGS(Options, Option)

private:
    void constmember(Type) const {}
    virtual void virtualmember() {}
    static void staticmember(int) {}
#ifdef Q_COMPILER_REF_QUALIFIERS
    void lvaluemember(const Type &&) & {}
    void clvaluemember(Type &&) const & {}
    void rvaluemember(const Type &) && {}
    void crvaluemember(Type &) const && {}
#endif

private Q_SLOTS:
    void basic();
    void strings();
    void enumsAndFlags();
    void pointers();
    void qtcore();
    void qtnetwork();
    void qtwidgets();
};
Q_DECLARE_OPERATORS_FOR_FLAGS(tst_ToString::Options)

#define CHECK_TOSTRING(expr, expected)  \
    do { \
        QScopedArrayPointer<char> actual(QTest::toString(expr)); \
        QCOMPARE(actual.data(), expected); \
    } while (false)

#define CHECK_PTR_TOSTRING(ptr, gccsym) \
    do { \
        bool failed = true; \
        QScopedArrayPointer<char> actual(QTest::toString(ptr)); \
        compare_sym(&failed, actual.data(), gccsym, ptr); \
        if (failed) QFAIL("Failure location"); \
    } while (false)

inline void compare_sym(bool *failed, char *actual, const char *gccsym, ...)
{
    QVERIFY(actual);

    QByteArray expected(3 + 2*sizeof(void*), Qt::Uninitialized);

    va_list ap;
    va_start(ap, gccsym);
    int n = qvsnprintf(expected.data(), expected.length(), "%p", ap);
    va_end(ap);

    expected.truncate(n);

    QCOMPARE(QByteArray(actual).left(n), expected);
    QVERIFY2(strlen(actual) == size_t(n) || actual[n] == ' ',
        "Actual: \"" + QByteArray(actual) +
        "\"; Expected: \"" + expected + '"');

    if (strlen(actual) > size_t(n)) {
        actual += n + 1;
        QVERIFY2(actual[0] == '<' && actual[strlen(actual) - 1] == '>', actual);
        actual[strlen(actual) - 1] = '\0';
        ++actual;

#if defined(Q_CC_GNU)
        QCOMPARE(QByteArray(actual).left(strlen(gccsym)), QByteArray(gccsym));
        QCOMPARE(actual[strlen(gccsym)], '+');
#else
        Q_UNUSED(gccsym);
#endif
    }

    *failed = false;
}

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

void tst_ToString::enumsAndFlags()
{
    CHECK_TOSTRING(Value1, "Value1");
    CHECK_TOSTRING(Value2, "Value2");

#if 0
    // flags aren't enums, so this doesn't work
    CHECK_TOSTRING(Option1, "Option1");
    CHECK_TOSTRING(Option2, "Option2");
#endif

    CHECK_TOSTRING(Qt::Sunday, "Sunday");
    CHECK_TOSTRING(Qt::Monday, "Monday");
    Qt::DayOfWeek dw = Qt::Tuesday;
    CHECK_TOSTRING(dw, "Tuesday");
}

void testfunction(int) {}
int gi = 0;
void tst_ToString::pointers()
{
    // pointer to variables
    int i;
    {
        int *p = 0;
        CHECK_PTR_TOSTRING(p, "");
        CHECK_PTR_TOSTRING(&i, "");
        CHECK_PTR_TOSTRING(&gi, "gi");
    }
    {
        void *v = 0;
        CHECK_PTR_TOSTRING(v, "");
        CHECK_PTR_TOSTRING((void*)&i, "");
        CHECK_PTR_TOSTRING((void*)&gi, "gi");
        CHECK_PTR_TOSTRING(1+(quint8*)&gi, "gi");
    }
    {
        const void *v = 0;
        CHECK_PTR_TOSTRING(v, "");
        CHECK_PTR_TOSTRING((const void*)&i, "");
        CHECK_PTR_TOSTRING((const void*)&gi, "gi");
        CHECK_PTR_TOSTRING(1+(const quint8*)&gi, "gi");
    }

    {
        void (*f)(int) = 0;
#ifdef Q_COMPILER_VARIADIC_TEMPLATES
        CHECK_PTR_TOSTRING(f, "");
        CHECK_PTR_TOSTRING(testfunction, "testfunction(int)");
        CHECK_PTR_TOSTRING(&testfunction, "testfunction(int)");
        CHECK_PTR_TOSTRING(&tst_ToString::staticmember, "tst_ToString::staticmember(int)");
#else
        char *null = 0;
        CHECK_TOSTRING(f, null);
        CHECK_TOSTRING(testfunction, null);
#endif
    }

    {
        void (tst_ToString:: *f)() = 0;
#if defined(Q_COMPILER_VARIADIC_TEMPLATES) && defined(Q_CC_GNU)
        CHECK_PTR_TOSTRING(f, "");
        CHECK_PTR_TOSTRING(&tst_ToString::pointers, "tst_ToString::pointers()");
        CHECK_PTR_TOSTRING(&tst_ToString::constmember, "tst_ToString::constmember(Type) const");

#  ifdef Q_COMPILER_REF_QUALIFIERS
        CHECK_PTR_TOSTRING(&tst_ToString::lvaluemember, "tst_ToString::lvaluemember(Type const&&) &");
        CHECK_PTR_TOSTRING(&tst_ToString::clvaluemember, "tst_ToString::clvaluemember(Type&&) const &");
        CHECK_PTR_TOSTRING(&tst_ToString::rvaluemember, "tst_ToString::rvaluemember(Type const&) &&");
        CHECK_PTR_TOSTRING(&tst_ToString::crvaluemember, "tst_ToString::crvaluemember(Type&) const &&");
#  endif

        // virtual members are a little more delicate
        QScopedArrayPointer<char> data(QTest::toString(&tst_ToString::virtualmember));
#  if defined(Q_PROCESSOR_X86) || defined(Q_PROCESSOR_SPARC) || defined(Q_PROCESSOR_POWER) \
    || defined(Q_PROCESSOR_MIPS) || defined(Q_PROCESSOR_ARM)
        QByteArray result(data.data());
        QVERIFY2(result.startsWith("tst_ToString::virtual"), result);
#  endif
#else
        char *null = 0;
        CHECK_TOSTRING(f, null);
        CHECK_TOSTRING(&tst_ToString::pointers, null);
        CHECK_TOSTRING(&tst_ToString::constmember, null);
        CHECK_TOSTRING(&tst_ToString::virtualmember, null);

#  ifdef Q_COMPILER_REF_QUALIFIERS
        CHECK_TOSTRING(&tst_ToString::lvaluemember, null);
        CHECK_TOSTRING(&tst_ToString::clvaluemember, null);
        CHECK_TOSTRING(&tst_ToString::rvaluemember, null);
        CHECK_TOSTRING(&tst_ToString::crvaluemember, null);
#  endif
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
