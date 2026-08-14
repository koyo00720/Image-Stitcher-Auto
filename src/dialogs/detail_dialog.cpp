#include "detail_dialog.h"
#include <QTableView>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include <QHeaderView>
#include <QSortFilterProxyModel>

#include <QStyledItemDelegate>
#include <QPainter>

class CenterAlignDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignCenter;   // ← 中央寄せ
    }
};

Detail_Dialog::Detail_Dialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("データ一覧");
    resize(620, 600);

    table = new QTableView(this);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    table->setSortingEnabled(true);
    table->setItemDelegate(new CenterAlignDelegate(table));

    buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* lay = new QVBoxLayout(this);
    lay->addWidget(table);
    lay->addWidget(buttons);
}


void Detail_Dialog::setModel(QAbstractItemModel* model)
{
    auto* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setSortRole(Qt::UserRole);
    proxy->setDynamicSortFilter(true);

    table->setModel(proxy);
    table->setSortingEnabled(true);
    table->horizontalHeader()->setSortIndicatorShown(true);

    table->resizeColumnsToContents();

    table->sortByColumn(0, Qt::AscendingOrder);
}

