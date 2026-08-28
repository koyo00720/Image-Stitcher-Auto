#include "detail_opti_dialog.h"
#include "ui_detail_opti_dialog.h"
#include <QHeaderView>
#include <QSplitter>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QEvent>

detail_opti_dialog::detail_opti_dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::detail_opti_dialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("最適化結果"));
    m_tableModel = new QStandardItemModel(this);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView_2->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView_2->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView_2->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView_2->setSortingEnabled(true);
    ui->tableView_2->horizontalHeader()->setSortIndicatorShown(true);
    connect(ui->pushButton, &QPushButton::clicked, this, &detail_opti_dialog::refreshUi);

    ui->groupBox->setMinimumHeight(80);
    ui->groupBox_2->setMinimumHeight(80);
    ui->groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->groupBox_2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QSplitter* splitter = new QSplitter(Qt::Vertical, ui->splitterHost);
    splitter->addWidget(ui->groupBox);
    splitter->addWidget(ui->groupBox_2);
    splitter->setSizes({160, 160});
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 0);  // 上側
    splitter->setStretchFactor(1, 1);  // 下側

    QVBoxLayout* layout = new QVBoxLayout(ui->splitterHost);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);
}

detail_opti_dialog::~detail_opti_dialog()
{
    delete ui;
}

void detail_opti_dialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        setWindowTitle(tr("最適化結果"));
        refreshUi();
    }
}

void detail_opti_dialog::setData(out_detail det,out_log log)
{
    one_line now;
    now.abst = det;
    now.detail = log;
    all_data.push_back(now);
}

std::vector<one_line> detail_opti_dialog::data() const
{
    return all_data;
}

void detail_opti_dialog::setAllData(const std::vector<one_line>& data)
{
    all_data = data;
    refreshUi();
}

void detail_opti_dialog::clearData()
{
    all_data.clear();
    refreshUi();
}

void detail_opti_dialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    refreshUi();
}

void detail_opti_dialog::refreshUi()
{
    const int N = all_data.size();

    m_tableModel->setColumnCount(9);
    m_tableModel->setRowCount(N);

    m_tableModel->setHeaderData(0, Qt::Horizontal, tr("部分/全体"));
    m_tableModel->setHeaderData(1, Qt::Horizontal, tr("画像 start ID"));
    m_tableModel->setHeaderData(2, Qt::Horizontal, tr("画像 end ID"));
    m_tableModel->setHeaderData(3, Qt::Horizontal, tr("計算回数"));
    m_tableModel->setHeaderData(4, Qt::Horizontal, tr("ループ回数"));
    m_tableModel->setHeaderData(5, Qt::Horizontal, "PAMI energy");
    m_tableModel->setHeaderData(6, Qt::Horizontal, tr("収束"));
    m_tableModel->setHeaderData(7, Qt::Horizontal, tr("低 SSIM エッジ数"));
    m_tableModel->setHeaderData(8, Qt::Horizontal, tr("最小 SSIM"));

    for (int n = 0; n < N; ++n) {
        // 全セルにデータを格納
        QString it1 = all_data[n].abst.PaAll ? tr("部分") : tr("全体");
        m_tableModel->setData(m_tableModel->index(n, 0), it1);
        m_tableModel->setData(m_tableModel->index(n, 1), all_data[n].abst.start);
        m_tableModel->setData(m_tableModel->index(n, 2), all_data[n].abst.end);
        m_tableModel->setData(m_tableModel->index(n, 3), all_data[n].abst.itr);
        m_tableModel->setData(m_tableModel->index(n, 4), all_data[n].abst.loop);
        m_tableModel->setData(m_tableModel->index(n, 5), all_data[n].abst.energy);
        QString it2 = all_data[n].abst.shuusoku ? QString("Yes") : QString("No");
        m_tableModel->setData(m_tableModel->index(n, 6), it2);
        m_tableModel->setData(m_tableModel->index(n, 7), all_data[n].abst.lowSSIM_num);
        m_tableModel->setData(m_tableModel->index(n, 8), all_data[n].abst.minSSIM);

        // 全セルを中央揃え
        for (int i = 0; i < 9; ++i) {
            m_tableModel->setData(m_tableModel->index(n, i), Qt::AlignCenter, Qt::TextAlignmentRole);
        }
    }

    ui->tableView->setModel(m_tableModel);

    // 選択状態が変化したとき
    connect(ui->tableView->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            &detail_opti_dialog::onCurrentRowChanged,
            Qt::UniqueConnection);


}

void detail_opti_dialog::onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);

    if (!current.isValid()) {
        return;
    }

    out_log data_sel = all_data[current.row()].detail;

    const int D = data_sel.ssim.size();

    QAbstractItemModel* oldModel = ui->tableView_2->model();
    auto* proxy = new QSortFilterProxyModel(ui->tableView_2);
    auto* m_tableModel_2 = new QStandardItemModel(proxy);
    m_tableModel_2->setColumnCount(3);
    m_tableModel_2->setRowCount(D);

    m_tableModel_2->setHeaderData(0, Qt::Horizontal, tr("edge 画像 ID 1"));
    m_tableModel_2->setHeaderData(1, Qt::Horizontal, tr("edge 画像 ID 2"));
    m_tableModel_2->setHeaderData(2, Qt::Horizontal, "SSIM");

    for (int n = 0; n < D; ++n) {
        const int edge1 = data_sel.edge1[n] + 1;
        const int edge2 = data_sel.edge2[n] + 1;
        const double ssim = data_sel.ssim[n];

        auto* item1 = new QStandardItem(QString::number(edge1));
        item1->setData(edge1, Qt::UserRole);
        m_tableModel_2->setItem(n, 0, item1);

        auto* item2 = new QStandardItem(QString::number(edge2));
        item2->setData(edge2, Qt::UserRole);
        m_tableModel_2->setItem(n, 1, item2);

        auto* item3 = new QStandardItem(QString::number(ssim, 'f', 4));
        item3->setData(ssim, Qt::UserRole);
        m_tableModel_2->setItem(n, 2, item3);

        // 全セルを中央揃え
        for (int i = 0; i < 3; ++i) {
            m_tableModel_2->setData(m_tableModel_2->index(n, i), Qt::AlignCenter, Qt::TextAlignmentRole);
        }
    }

    proxy->setSourceModel(m_tableModel_2);
    proxy->setSortRole(Qt::UserRole);
    proxy->setDynamicSortFilter(true);

    ui->tableView_2->setModel(proxy);
    ui->tableView_2->resizeColumnsToContents();
    ui->tableView_2->sortByColumn(0, Qt::AscendingOrder);

    if (oldModel) {
        oldModel->deleteLater();
    }

}
