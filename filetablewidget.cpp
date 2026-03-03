#include "filetablewidget.h"

#include <QDropEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QDebug>
#include <utility>

FileTableWidget::FileTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    setDragEnabled(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(true);

    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);

    setSortingEnabled(false);

    horizontalHeader()->setSectionsMovable(false);
    verticalHeader()->setSectionsMovable(false);
}

QList<FileTableWidget::RowSnapshot> FileTableWidget::snapshotAllRows() const
{
    QList<RowSnapshot> rows;
    const int rCount = rowCount();
    const int cCount = columnCount();

    rows.reserve(rCount);

    for (int r = 0; r < rCount; ++r) {
        RowSnapshot snap;
        snap.items.resize(cCount);
        snap.cellWidgets.resize(cCount);

        for (int c = 0; c < cCount; ++c) {
            QTableWidgetItem *it = item(r, c);
            snap.items[c] = (it ? it->clone() : nullptr);

            // setCellWidget を使っている場合のため（通常は nullptr）
            snap.cellWidgets[c] = cellWidget(r, c);
        }

        rows.append(snap);
    }

    return rows;
}

void FileTableWidget::rebuildFromSnapshots(const QList<RowSnapshot> &rows)
{
    const int cCount = columnCount();

    // 既存行を消す（列・ヘッダは維持）
    clearContents();
    setRowCount(0);

    for (int r = 0; r < rows.size(); ++r) {
        insertRow(r);

        for (int c = 0; c < cCount; ++c) {
            if (rows[r].items[c]) {
                setItem(r, c, rows[r].items[c]->clone()); // 再cloneして所有権を table に渡す
            }
        }
    }
}

void FileTableWidget::freeSnapshots(QList<RowSnapshot> &rows)
{
    for (auto &row : rows) {
        for (QTableWidgetItem *const it : std::as_const(row.items)) {
            delete it;
        }
        row.items.clear();
        row.cellWidgets.clear();
    }
}

void FileTableWidget::dropEvent(QDropEvent *event)
{
    QObject *srcObj = event->source();
    const bool internalDrag = (srcObj == this || srcObj == viewport());

    if (!internalDrag) {
        QTableWidget::dropEvent(event);
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif

    if (!selectionModel()) {
        event->ignore();
        return;
    }

    // --- 選択行を取得（複数対応）---
    QList<int> selectedRowsList;
    for (const QModelIndex &idx : selectionModel()->selectedRows()) {
        selectedRowsList.append(idx.row());
    }

    if (selectedRowsList.isEmpty()) {
        event->ignore();
        return;
    }

    std::sort(selectedRowsList.begin(), selectedRowsList.end());
    selectedRowsList.erase(std::unique(selectedRowsList.begin(), selectedRowsList.end()),
                           selectedRowsList.end());

    // --- ドロップ先を常に「行間」に正規化 ---
    int dstRow = -1;
    const int hitRow = rowAt(pos.y());

    if (hitRow < 0) {
        dstRow = rowCount(); // 下側なら末尾
    } else {
        const QRect rr = visualRect(model()->index(hitRow, 0));
        const auto dip = dropIndicatorPosition();

        switch (dip) {
        case QAbstractItemView::AboveItem:
            dstRow = hitRow;
            break;

        case QAbstractItemView::BelowItem:
            dstRow = hitRow + 1;
            break;

        case QAbstractItemView::OnItem:
            dstRow = (pos.y() <= rr.center().y()) ? hitRow : (hitRow + 1);
            break;

        case QAbstractItemView::OnViewport:
        default:
            dstRow = (pos.y() <= rr.center().y()) ? hitRow : (hitRow + 1);
            break;
        }
    }

    // --- 全行スナップショット ---
    QList<RowSnapshot> rows = snapshotAllRows();
    if (rows.isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }

    // --- 選択行を「元順のまま」抜き出す ---
    QList<RowSnapshot> movingRows;
    movingRows.reserve(selectedRowsList.size());

    for (int r : selectedRowsList) {
        if (r >= 0 && r < rows.size()) {
            movingRows.append(rows[r]);
        }
    }

    // --- rows から選択行を削除（降順）---
    for (int i = selectedRowsList.size() - 1; i >= 0; --i) {
        const int r = selectedRowsList[i];
        if (r >= 0 && r < rows.size()) {
            rows.removeAt(r);
        }
    }

    // --- 挿入先補正 ---
    int removedBeforeDst = 0;
    for (int r : std::as_const(selectedRowsList)) {
        if (r < dstRow) ++removedBeforeDst;
    }

    int dstRowAdjusted = dstRow - removedBeforeDst;

    if (dstRowAdjusted < 0) dstRowAdjusted = 0;
    if (dstRowAdjusted > rows.size()) dstRowAdjusted = rows.size();

    // --- まとめて挿入（選択順維持）---
    for (int i = 0; i < movingRows.size(); ++i) {
        rows.insert(dstRowAdjusted + i, movingRows[i]);
    }

    // --- UI再構築 ---
    rebuildFromSnapshots(rows);

    // --- 選択状態を復元（移動後の複数行）---
    clearSelection();
    for (int i = 0; i < movingRows.size(); ++i) {
        const int r = dstRowAdjusted + i;
        if (r >= 0 && r < rowCount()) {
            selectRow(r); // これだと前の選択が消える環境がある
        }
    }

    // selectRowが単独選択になる場合は selectionModel で明示的に選択
    if (selectionModel()) {
        clearSelection();
        for (int i = 0; i < movingRows.size(); ++i) {
            const int r = dstRowAdjusted + i;
            if (r >= 0 && r < rowCount()) {
                selectionModel()->select(model()->index(r, 0),
                                         QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        }
        setCurrentCell(dstRowAdjusted, 0);
    }

    // スナップショット解放（rebuild側で clone 済み前提）
    freeSnapshots(rows);

    // QtのMove後処理による削除を防ぐ
    event->setDropAction(Qt::CopyAction);
    event->accept();

    emit rowsReordered();
}
