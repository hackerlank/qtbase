/****************************************************************************
**
** Copyright (C) 2011 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Stephen Kelly <stephen.kelly@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qidentityproxymodel.h"

#ifndef QT_NO_IDENTITYPROXYMODEL

#include "qitemselectionmodel.h"
#include <private/qabstractproxymodel_p.h>

QT_BEGIN_NAMESPACE

class QIdentityProxyModelPrivate : public QAbstractProxyModelPrivate
{
    QIdentityProxyModelPrivate()
    {

    }

    Q_DECLARE_PUBLIC(QIdentityProxyModel)

    QList<QPersistentModelIndex> layoutChangePersistentIndexes;
    QModelIndexList proxyIndexes;

    void _q_sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end);
    void _q_sourceRowsInserted(const QModelIndex &parent, int start, int end);
    void _q_sourceRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void _q_sourceRowsRemoved(const QModelIndex &parent, int start, int end);
    void _q_sourceRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest);
    void _q_sourceRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest);

    void _q_sourceColumnsAboutToBeInserted(const QModelIndex &parent, int start, int end);
    void _q_sourceColumnsInserted(const QModelIndex &parent, int start, int end);
    void _q_sourceColumnsAboutToBeRemoved(const QModelIndex &parent, int start, int end);
    void _q_sourceColumnsRemoved(const QModelIndex &parent, int start, int end);
    void _q_sourceColumnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest);
    void _q_sourceColumnsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest);

    void _q_sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
    void _q_sourceHeaderDataChanged(Qt::Orientation orientation, int first, int last);

    void _q_sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint);
    void _q_sourceLayoutChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint);
    void _q_sourceModelAboutToBeReset();
    void _q_sourceModelReset();

};

/*!
    \since 4.8
    \class QIdentityProxyModel
    \inmodule QtCore
    \brief The QIdentityProxyModel class proxies its source model unmodified

    \ingroup model-view

    QIdentityProxyModel can be used to forward the structure of a source model exactly, with no sorting, filtering or other transformation.
    This is similar in concept to an identity matrix where A.I = A.

    Because it does no sorting or filtering, this class is most suitable to proxy models which transform the data() of the source model.
    For example, a proxy model could be created to define the font used, or the background colour, or the tooltip etc. This removes the
    need to implement all data handling in the same class that creates the structure of the model, and can also be used to create
    re-usable components.

    This also provides a way to change the data in the case where a source model is supplied by a third party which can not be modified.

    \snippet code/src_gui_itemviews_qidentityproxymodel.cpp 0

    \sa QAbstractProxyModel, {Model/View Programming}, QAbstractItemModel

*/

/*!
    Constructs an identity model with the given \a parent.
*/
QIdentityProxyModel::QIdentityProxyModel(QObject* parent)
  : QAbstractProxyModel(*new QIdentityProxyModelPrivate, parent)
{

}

/*!
    \internal
 */
QIdentityProxyModel::QIdentityProxyModel(QIdentityProxyModelPrivate &dd, QObject* parent)
  : QAbstractProxyModel(dd, parent)
{

}

/*!
    Destroys this identity model.
*/
QIdentityProxyModel::~QIdentityProxyModel()
{
}

/*!
    \reimp
 */
int QIdentityProxyModel::columnCount(const QModelIndex& parent) const
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(const QIdentityProxyModel);
    return d->model->columnCount(mapToSource(parent));
}

/*!
    \reimp
 */
bool QIdentityProxyModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(QIdentityProxyModel);
    return d->model->dropMimeData(data, action, row, column, mapToSource(parent));
}

/*!
    \reimp
 */
QModelIndex QIdentityProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(const QIdentityProxyModel);
    const QModelIndex sourceParent = mapToSource(parent);
    const QModelIndex sourceIndex = d->model->index(row, column, sourceParent);
    return mapFromSource(sourceIndex);
}

/*!
    \reimp
 */
QModelIndex QIdentityProxyModel::sibling(int row, int column, const QModelIndex &idx) const
{
    Q_D(const QIdentityProxyModel);
    return mapFromSource(d->model->sibling(row, column, mapToSource(idx)));
}

/*!
    \reimp
 */
bool QIdentityProxyModel::insertColumns(int column, int count, const QModelIndex& parent)
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(QIdentityProxyModel);
    return d->model->insertColumns(column, count, mapToSource(parent));
}

/*!
    \reimp
 */
bool QIdentityProxyModel::insertRows(int row, int count, const QModelIndex& parent)
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(QIdentityProxyModel);
    return d->model->insertRows(row, count, mapToSource(parent));
}

/*!
    \reimp
 */
QModelIndex QIdentityProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    Q_D(const QIdentityProxyModel);
    if (!d->model || !sourceIndex.isValid())
        return QModelIndex();

    Q_ASSERT(sourceIndex.model() == d->model);
    return createIndex(sourceIndex.row(), sourceIndex.column(), sourceIndex.internalPointer());
}

/*!
    \reimp
 */
QItemSelection QIdentityProxyModel::mapSelectionFromSource(const QItemSelection& selection) const
{
    Q_D(const QIdentityProxyModel);
    QItemSelection proxySelection;

    if (!d->model)
        return proxySelection;

    QItemSelection::const_iterator it = selection.constBegin();
    const QItemSelection::const_iterator end = selection.constEnd();
    proxySelection.reserve(selection.count());
    for ( ; it != end; ++it) {
        Q_ASSERT(it->model() == d->model);
        const QItemSelectionRange range(mapFromSource(it->topLeft()), mapFromSource(it->bottomRight()));
        proxySelection.append(range);
    }

    return proxySelection;
}

/*!
    \reimp
 */
QItemSelection QIdentityProxyModel::mapSelectionToSource(const QItemSelection& selection) const
{
    Q_D(const QIdentityProxyModel);
    QItemSelection sourceSelection;

    if (!d->model)
        return sourceSelection;

    QItemSelection::const_iterator it = selection.constBegin();
    const QItemSelection::const_iterator end = selection.constEnd();
    sourceSelection.reserve(selection.count());
    for ( ; it != end; ++it) {
        Q_ASSERT(it->model() == this);
        const QItemSelectionRange range(mapToSource(it->topLeft()), mapToSource(it->bottomRight()));
        sourceSelection.append(range);
    }

    return sourceSelection;
}

/*!
    \reimp
 */
QModelIndex QIdentityProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    Q_D(const QIdentityProxyModel);
    if (!d->model || !proxyIndex.isValid())
        return QModelIndex();
    Q_ASSERT(proxyIndex.model() == this);
    return d->model->createIndex(proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
}

/*!
    \reimp
 */
QModelIndexList QIdentityProxyModel::match(const QModelIndex& start, int role, const QVariant& value, int hits, Qt::MatchFlags flags) const
{
    Q_D(const QIdentityProxyModel);
    Q_ASSERT(start.isValid() ? start.model() == this : true);
    if (!d->model)
        return QModelIndexList();

    const QModelIndexList sourceList = d->model->match(mapToSource(start), role, value, hits, flags);
    QModelIndexList::const_iterator it = sourceList.constBegin();
    const QModelIndexList::const_iterator end = sourceList.constEnd();
    QModelIndexList proxyList;
    proxyList.reserve(sourceList.count());
    for ( ; it != end; ++it)
        proxyList.append(mapFromSource(*it));
    return proxyList;
}

/*!
    \reimp
 */
QModelIndex QIdentityProxyModel::parent(const QModelIndex& child) const
{
    Q_ASSERT(child.isValid() ? child.model() == this : true);
    const QModelIndex sourceIndex = mapToSource(child);
    const QModelIndex sourceParent = sourceIndex.parent();
    return mapFromSource(sourceParent);
}

/*!
    \reimp
 */
bool QIdentityProxyModel::removeColumns(int column, int count, const QModelIndex& parent)
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(QIdentityProxyModel);
    return d->model->removeColumns(column, count, mapToSource(parent));
}

/*!
    \reimp
 */
bool QIdentityProxyModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(QIdentityProxyModel);
    return d->model->removeRows(row, count, mapToSource(parent));
}

/*!
    \reimp
 */
int QIdentityProxyModel::rowCount(const QModelIndex& parent) const
{
    Q_ASSERT(parent.isValid() ? parent.model() == this : true);
    Q_D(const QIdentityProxyModel);
    return d->model->rowCount(mapToSource(parent));
}

/*!
    \reimp
 */
QVariant QIdentityProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_D(const QIdentityProxyModel);
    return d->model->headerData(section, orientation, role);
}

/*!
    \reimp
 */
void QIdentityProxyModel::setSourceModel(QAbstractItemModel* newSourceModel)
{
    beginResetModel();

    if (sourceModel()) {
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::rowsAboutToBeInserted,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeInserted);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::rowsInserted,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsInserted);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeRemoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::rowsRemoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsRemoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::rowsAboutToBeMoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeMoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::rowsMoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsMoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::columnsAboutToBeInserted,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeInserted);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::columnsInserted,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsInserted);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::columnsAboutToBeRemoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeRemoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::columnsRemoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsRemoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::columnsAboutToBeMoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeMoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::columnsMoved,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsMoved);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::modelAboutToBeReset,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceModelAboutToBeReset);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::modelReset,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceModelReset);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::dataChanged,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceDataChanged);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::headerDataChanged,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceHeaderDataChanged);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::layoutAboutToBeChanged,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceLayoutAboutToBeChanged);
        QObjectPrivate::disconnect(sourceModel(), &QAbstractItemModel::layoutChanged,
                                   d_func(), &QIdentityProxyModelPrivate::_q_sourceLayoutChanged);
    }

    QAbstractProxyModel::setSourceModel(newSourceModel);

    if (sourceModel()) {
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeInserted,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeInserted);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::rowsInserted,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsInserted);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeRemoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::rowsRemoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsRemoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeMoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeMoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::rowsMoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceRowsMoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::columnsAboutToBeInserted,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeInserted);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::columnsInserted,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsInserted);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::columnsAboutToBeRemoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeRemoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::columnsRemoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsRemoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::columnsAboutToBeMoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeMoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::columnsMoved,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceColumnsMoved);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::modelAboutToBeReset,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceModelAboutToBeReset);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::modelReset,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceModelReset);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::dataChanged,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceDataChanged);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::headerDataChanged,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceHeaderDataChanged);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::layoutAboutToBeChanged,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceLayoutAboutToBeChanged);
        QObjectPrivate::connect(sourceModel(), &QAbstractItemModel::layoutChanged,
                                d_func(), &QIdentityProxyModelPrivate::_q_sourceLayoutChanged);
    }

    endResetModel();
}

void QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->beginInsertColumns(q->mapFromSource(parent), start, end);
}

void QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest)
{
    Q_ASSERT(sourceParent.isValid() ? sourceParent.model() == model : true);
    Q_ASSERT(destParent.isValid() ? destParent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->beginMoveColumns(q->mapFromSource(sourceParent), sourceStart, sourceEnd, q->mapFromSource(destParent), dest);
}

void QIdentityProxyModelPrivate::_q_sourceColumnsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->beginRemoveColumns(q->mapFromSource(parent), start, end);
}

void QIdentityProxyModelPrivate::_q_sourceColumnsInserted(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    q->endInsertColumns();
}

void QIdentityProxyModelPrivate::_q_sourceColumnsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest)
{
    Q_ASSERT(sourceParent.isValid() ? sourceParent.model() == model : true);
    Q_ASSERT(destParent.isValid() ? destParent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    Q_UNUSED(sourceParent)
    Q_UNUSED(sourceStart)
    Q_UNUSED(sourceEnd)
    Q_UNUSED(destParent)
    Q_UNUSED(dest)
    q->endMoveColumns();
}

void QIdentityProxyModelPrivate::_q_sourceColumnsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    q->endRemoveColumns();
}

void QIdentityProxyModelPrivate::_q_sourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_ASSERT(topLeft.isValid() ? topLeft.model() == model : true);
    Q_ASSERT(bottomRight.isValid() ? bottomRight.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->dataChanged(q->mapFromSource(topLeft), q->mapFromSource(bottomRight), roles);
}

void QIdentityProxyModelPrivate::_q_sourceHeaderDataChanged(Qt::Orientation orientation, int first, int last)
{
    Q_Q(QIdentityProxyModel);
    q->headerDataChanged(orientation, first, last);
}

void QIdentityProxyModelPrivate::_q_sourceLayoutAboutToBeChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_Q(QIdentityProxyModel);

    const auto proxyPersistentIndexes = q->persistentIndexList();
    for (const QPersistentModelIndex &proxyPersistentIndex : proxyPersistentIndexes) {
        proxyIndexes << proxyPersistentIndex;
        Q_ASSERT(proxyPersistentIndex.isValid());
        const QPersistentModelIndex srcPersistentIndex = q->mapToSource(proxyPersistentIndex);
        Q_ASSERT(srcPersistentIndex.isValid());
        layoutChangePersistentIndexes << srcPersistentIndex;
    }

    QList<QPersistentModelIndex> parents;
    parents.reserve(sourceParents.size());
    for (const QPersistentModelIndex &parent : sourceParents) {
        if (!parent.isValid()) {
            parents << QPersistentModelIndex();
            continue;
        }
        const QModelIndex mappedParent = q->mapFromSource(parent);
        Q_ASSERT(mappedParent.isValid());
        parents << mappedParent;
    }

    q->layoutAboutToBeChanged(parents, hint);
}

void QIdentityProxyModelPrivate::_q_sourceLayoutChanged(const QList<QPersistentModelIndex> &sourceParents, QAbstractItemModel::LayoutChangeHint hint)
{
    Q_Q(QIdentityProxyModel);

    for (int i = 0; i < proxyIndexes.size(); ++i) {
        q->changePersistentIndex(proxyIndexes.at(i), q->mapFromSource(layoutChangePersistentIndexes.at(i)));
    }

    layoutChangePersistentIndexes.clear();
    proxyIndexes.clear();

    QList<QPersistentModelIndex> parents;
    parents.reserve(sourceParents.size());
    for (const QPersistentModelIndex &parent : sourceParents) {
        if (!parent.isValid()) {
            parents << QPersistentModelIndex();
            continue;
        }
        const QModelIndex mappedParent = q->mapFromSource(parent);
        Q_ASSERT(mappedParent.isValid());
        parents << mappedParent;
    }

    q->layoutChanged(parents, hint);
}

void QIdentityProxyModelPrivate::_q_sourceModelAboutToBeReset()
{
    Q_Q(QIdentityProxyModel);
    q->beginResetModel();
}

void QIdentityProxyModelPrivate::_q_sourceModelReset()
{
    Q_Q(QIdentityProxyModel);
    q->endResetModel();
}

void QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeInserted(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->beginInsertRows(q->mapFromSource(parent), start, end);
}

void QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest)
{
    Q_ASSERT(sourceParent.isValid() ? sourceParent.model() == model : true);
    Q_ASSERT(destParent.isValid() ? destParent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->beginMoveRows(q->mapFromSource(sourceParent), sourceStart, sourceEnd, q->mapFromSource(destParent), dest);
}

void QIdentityProxyModelPrivate::_q_sourceRowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    q->beginRemoveRows(q->mapFromSource(parent), start, end);
}

void QIdentityProxyModelPrivate::_q_sourceRowsInserted(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    q->endInsertRows();
}

void QIdentityProxyModelPrivate::_q_sourceRowsMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destParent, int dest)
{
    Q_ASSERT(sourceParent.isValid() ? sourceParent.model() == model : true);
    Q_ASSERT(destParent.isValid() ? destParent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    Q_UNUSED(sourceParent)
    Q_UNUSED(sourceStart)
    Q_UNUSED(sourceEnd)
    Q_UNUSED(destParent)
    Q_UNUSED(dest)
    q->endMoveRows();
}

void QIdentityProxyModelPrivate::_q_sourceRowsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_ASSERT(parent.isValid() ? parent.model() == model : true);
    Q_Q(QIdentityProxyModel);
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    q->endRemoveRows();
}

QT_END_NAMESPACE

#include "moc_qidentityproxymodel.cpp"

#endif // QT_NO_IDENTITYPROXYMODEL
