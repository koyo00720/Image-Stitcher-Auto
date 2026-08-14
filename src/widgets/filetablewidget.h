#ifndef FILETABLEWIDGET_H
#define FILETABLEWIDGET_H

#include <QTableWidget>
#include <QIcon>

class FileTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit FileTableWidget(QWidget *parent = nullptr);

signals:
    void rowsReordered();

protected:
    void dropEvent(QDropEvent *event) override;

private:
    struct RowSnapshot {
        QVector<QTableWidgetItem*> items;  // cloneしたアイテム
        QVector<QWidget*> cellWidgets;     // 必要なら移す
    };

    QList<RowSnapshot> snapshotAllRows() const;
    void rebuildFromSnapshots(const QList<RowSnapshot> &rows);
    void freeSnapshots(QList<RowSnapshot> &rows);
};

#endif // FILETABLEWIDGET_H
