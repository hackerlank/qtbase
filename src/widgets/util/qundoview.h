/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QUNDOVIEW_H
#define QUNDOVIEW_H

#include <QtWidgets/qlistview.h>
#include <QtCore/qstring.h>

#ifndef QT_NO_UNDOVIEW

QT_BEGIN_NAMESPACE

class QUndoViewPrivate;
class QUndoStack;
class QUndoGroup;
class QIcon;


class Q_WIDGETS_EXPORT QUndoView : public QListView
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QUndoView)
    Q_PROPERTY(QString emptyLabel READ emptyLabel WRITE setEmptyLabel)
    Q_PROPERTY(QIcon cleanIcon READ cleanIcon WRITE setCleanIcon)

public:
    explicit QUndoView(QWidget *parent = 0);
    explicit QUndoView(QUndoStack *stack, QWidget *parent = 0);
#ifndef QT_NO_UNDOGROUP
    explicit QUndoView(QUndoGroup *group, QWidget *parent = 0);
#endif
    ~QUndoView();

    QUndoStack *stack() const;
#ifndef QT_NO_UNDOGROUP
    QUndoGroup *group() const;
#endif

    void setEmptyLabel(const QString &label);
    QString emptyLabel() const;

    void setCleanIcon(const QIcon &icon);
    QIcon cleanIcon() const;

public Q_SLOTS:
    void setStack(QUndoStack *stack);
#ifndef QT_NO_UNDOGROUP
    void setGroup(QUndoGroup *group);
#endif

private:
    Q_DISABLE_COPY(QUndoView)
};

QT_END_NAMESPACE

#endif // QT_NO_UNDOVIEW
#endif // QUNDOVIEW_H
