#include "detail_dialog.h"
#include <QTableView>
#include <QDialogButtonBox>
#include <QVBoxLayout>

#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QEvent>
#include <QCoreApplication>

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
    setWindowTitle(tr("データ一覧"));
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

void Detail_Dialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        setWindowTitle(tr("データ一覧"));
        retranslateHeaders();
    }
}

void Detail_Dialog::retranslateHeaders()
{
    QAbstractItemModel* model = table ? table->model() : nullptr;
    if (!model) {
        return;
    }

    QStringList headers;
    if (model->columnCount() == 7) {
        headers = {
            QCoreApplication::translate("MainWindow", "画像1"),
            QCoreApplication::translate("MainWindow", "画像2"),
            QCoreApplication::translate("MainWindow", "計算回数"),
            QCoreApplication::translate("MainWindow", "安定性"),
            QCoreApplication::translate("MainWindow", "位相相関スコア"),
            QStringLiteral("SSIM"),
            QCoreApplication::translate("MainWindow", "品質")
        };
    } else if (model->columnCount() == 13) {
        headers = {
            QCoreApplication::translate("MainWindow", "画像1"),
            QCoreApplication::translate("MainWindow", "画像2"),
            QCoreApplication::translate("MainWindow", "状態"),
            QCoreApplication::translate("MainWindow", "計算回数"),
            QCoreApplication::translate("MainWindow", "安定性"),
            QCoreApplication::translate("MainWindow", "位相相関スコア"),
            QCoreApplication::translate("MainWindow", "測定SSIM"),
            QCoreApplication::translate("MainWindow", "計算後SSIM"),
            QCoreApplication::translate("MainWindow", "測定dx"),
            QCoreApplication::translate("MainWindow", "測定dy"),
            QCoreApplication::translate("MainWindow", "最適dx"),
            QCoreApplication::translate("MainWindow", "最適dy"),
            QCoreApplication::translate("MainWindow", "残差")
        };
    }

    for (int column = 0; column < headers.size(); ++column) {
        model->setHeaderData(column, Qt::Horizontal, headers[column]);
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        for (int column = 0; column < model->columnCount(); ++column) {
            const QModelIndex index = model->index(row, column);
            const QString key = model->data(index, Qt::UserRole + 1).toString();
            QString translated;
            if (key == QStringLiteral("stable")) {
                translated = QCoreApplication::translate("MainWindow", "安定");
            } else if (key == QStringLiteral("unstable")) {
                translated = QCoreApplication::translate("MainWindow", "不安定");
            } else if (key == QStringLiteral("good")) {
                translated = QCoreApplication::translate("MainWindow", "良");
            } else if (key == QStringLiteral("bad")) {
                translated = QCoreApplication::translate("MainWindow", "不良");
            } else if (key == QStringLiteral("calculation_failed")) {
                translated = QCoreApplication::translate("MainWindow", "計算失敗");
            } else if (key == QStringLiteral("below_threshold")) {
                translated = QCoreApplication::translate("MainWindow", "しきい値未満");
            } else if (key == QStringLiteral("accepted")) {
                translated = QCoreApplication::translate("MainWindow", "採用");
            } else if (key == QStringLiteral("excluded")) {
                translated = QCoreApplication::translate("MainWindow", "除外");
            }
            if (!translated.isEmpty()) {
                model->setData(index, translated, Qt::DisplayRole);
            }
        }
    }
}


void Detail_Dialog::setModel(QAbstractItemModel* model)
{
    auto* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setSortRole(Qt::UserRole);
    proxy->setDynamicSortFilter(true);

    table->setModel(proxy);
    retranslateHeaders();
    table->setSortingEnabled(true);
    table->horizontalHeader()->setSortIndicatorShown(true);

    table->resizeColumnsToContents();

    table->sortByColumn(0, Qt::AscendingOrder);
}

