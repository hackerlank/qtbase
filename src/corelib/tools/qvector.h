/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Intel Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QVECTOR_H
#define QVECTOR_H

#include <QtCore/qgenericarray.h>
#include <QtCore/qlist.h>
#include <QtCore/qhashfunctions.h>
#include <vector>

#ifdef Q_COMPILER_INITIALIZER_LISTS
#include <initializer_list>
#endif

#include <algorithm>

QT_BEGIN_NAMESPACE

template <typename T>
class QVector : public QGenericArray<T>
{
    typedef typename QGenericArray<T>::parameter_type parameter_type;

    explicit QVector(const QArrayDataPointer<T> &dd)
        : QGenericArray<T>(dd)
    { }
public:
    QVector() Q_DECL_NOTHROW {}
    explicit QVector(int n) : QGenericArray<T>(size_t(n)) {}
    QVector(int size, parameter_type t) : QGenericArray<T>(size, t) {}
    QVector(const QVector<T> &v) : QGenericArray<T>(v) {}
    // ~QVector() = default;
    QVector<T> &operator=(const QVector<T> &v)
    { QGenericArray<T>::operator =(v); return *this; }
#ifdef Q_COMPILER_RVALUE_REFS
    QVector(QVector<T> &&other) Q_DECL_NOTHROW : QGenericArray<T>(std::move(other)) {}
    QVector<T> &operator=(QVector<T> &&other) Q_DECL_NOTHROW
    { QGenericArray<T>::operator =(std::move(other)); return *this; }
#endif
#ifdef Q_COMPILER_INITIALIZER_LISTS
    QVector(std::initializer_list<T> args) : QGenericArray<T>(args) {}
#endif

    void swap(QVector<T> &other) { QGenericArray<T>::swap(other); }
    bool isSharedWith(const QVector &other) const { return QGenericArray<T>::isSharedWith(other); }

    QVector<T> &fill(parameter_type t, int size = -1)
    {
        QGenericArray<T>::fill(t, size);
        return *this;
    }

    QVector<T> mid(int pos, int len = -1) const
    {
        QArrayDataPointer<T> r = QGenericArray<T>::mid(pos, len);
        return QVector(r);
    }

    QVector<T> &operator+=(const QVector<T> &l)
    {
        if (this->d.isNull())
            *this = l;
        else
            this->append(l.constBegin(), l.constEnd());
        return *this;
    }

    inline QVector<T> operator+(const QVector<T> &l) const
    { QVector n = *this; n += l; return n; }
    inline QVector<T> &operator+=(const T &t)
    { this->append(t); return *this; }
    inline QVector<T> &operator<< (const T &t)
    { this->append(t); return *this; }
    inline QVector<T> &operator<<(const QVector<T> &l)
    { *this += l; return *this; }

    QList<T> toList() const;
    static QVector<T> fromList(const QList<T> &list);

    static inline QVector<T> fromStdVector(const std::vector<T> &vector)
    { QVector<T> tmp; tmp.reserve(int(vector.size())); std::copy(vector.begin(), vector.end(), std::back_inserter(tmp)); return tmp; }
    inline std::vector<T> toStdVector() const
    { return std::vector<T>(this->begin(), this->end()); }
};

template <typename T>
Q_OUTOFLINE_TEMPLATE QList<T> QVector<T>::toList() const
{
    QList<T> result;
    result.reserve(this->size());
    for (int i = 0; i < this->size(); ++i)
        result.append(this->at(i));
    return result;
}

template <typename T>
Q_OUTOFLINE_TEMPLATE QVector<T> QList<T>::toVector() const
{
    QVector<T> result(this->size());
    for (int i = 0; i < this->size(); ++i)
        result[i] = at(i);
    return result;
}

template <typename T>
QVector<T> QVector<T>::fromList(const QList<T> &list)
{
    return list.toVector();
}

template <typename T>
QList<T> QList<T>::fromVector(const QVector<T> &vector)
{
    return vector.toList();
}

Q_DECLARE_SEQUENTIAL_ITERATOR(Vector)
Q_DECLARE_MUTABLE_SEQUENTIAL_ITERATOR(Vector)

template <typename T>
uint qHash(const QVector<T> &key, uint seed = 0)
    Q_DECL_NOEXCEPT_EXPR(noexcept(qHashRange(key.cbegin(), key.cend(), seed)))
{
    return qHashRange(key.cbegin(), key.cend(), seed);
}

template <typename T>
bool operator<(const QVector<T> &lhs, const QVector<T> &rhs)
    Q_DECL_NOEXCEPT_EXPR(noexcept(std::lexicographical_compare(lhs.begin(), lhs.end(),
                                                               rhs.begin(), rhs.end())))
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(),
                                        rhs.begin(), rhs.end());
}

template <typename T>
inline bool operator>(const QVector<T> &lhs, const QVector<T> &rhs)
    Q_DECL_NOEXCEPT_EXPR(noexcept(lhs < rhs))
{
    return rhs < lhs;
}

template <typename T>
inline bool operator<=(const QVector<T> &lhs, const QVector<T> &rhs)
    Q_DECL_NOEXCEPT_EXPR(noexcept(lhs < rhs))
{
    return !(lhs > rhs);
}

template <typename T>
inline bool operator>=(const QVector<T> &lhs, const QVector<T> &rhs)
    Q_DECL_NOEXCEPT_EXPR(noexcept(lhs < rhs))
{
    return !(lhs < rhs);
}

/*
   ### Qt 5:
   ### This needs to be removed for next releases of Qt. It is a workaround for vc++ because
   ### Qt exports QPolygon and QPolygonF that inherit QVector<QPoint> and
   ### QVector<QPointF> respectively.
*/

#ifdef Q_CC_MSVC
QT_BEGIN_INCLUDE_NAMESPACE
#include <QtCore/qpoint.h>
QT_END_INCLUDE_NAMESPACE

#if defined(QT_BUILD_CORE_LIB)
#define Q_TEMPLATE_EXTERN
#else
#define Q_TEMPLATE_EXTERN extern
#endif
Q_TEMPLATE_EXTERN template class Q_CORE_EXPORT QVector<QPointF>;
Q_TEMPLATE_EXTERN template class Q_CORE_EXPORT QVector<QPoint>;
#endif

QT_END_NAMESPACE

#endif // QVECTOR_H
