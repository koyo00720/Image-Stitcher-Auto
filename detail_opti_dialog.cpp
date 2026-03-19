#include "detail_opti_dialog.h"
#include "ui_detail_opti_dialog.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QSizePolicy>

detail_opti_dialog::detail_opti_dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::detail_opti_dialog)
{
    ui->setupUi(this);
    setWindowTitle("最適化結果");
    m_tableModel = new QStandardItemModel(this);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
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

void detail_opti_dialog::setData(out_detail det,out_log log)
{
    one_line now;
    now.abst = det;
    now.detail = log;
    all_data.push_back(now);
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

    m_tableModel->setHeaderData(0, Qt::Horizontal, "部分/全体");
    m_tableModel->setHeaderData(1, Qt::Horizontal, "画像 start ID");
    m_tableModel->setHeaderData(2, Qt::Horizontal, "画像 end ID");
    m_tableModel->setHeaderData(3, Qt::Horizontal, "計算回数");
    m_tableModel->setHeaderData(4, Qt::Horizontal, "ループ回数");
    m_tableModel->setHeaderData(5, Qt::Horizontal, "PAMI energy");
    m_tableModel->setHeaderData(6, Qt::Horizontal, "収束");
    m_tableModel->setHeaderData(7, Qt::Horizontal, "低 SSIM エッジ数");
    m_tableModel->setHeaderData(8, Qt::Horizontal, "最小 SSIM");

    for (int n = 0; n < N; ++n) {
        // 全セルにデータを格納
        QString it1 = all_data[n].abst.PaAll ? QString("部分") : QString("全体");
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
            &detail_opti_dialog::onCurrentRowChanged);


}

void detail_opti_dialog::onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);

    if (!current.isValid()) {
        return;
    }

    out_log data_sel = all_data[current.row()].detail;

    const int D = data_sel.ssim.size();

    QStandardItemModel* m_tableModel_2 = new QStandardItemModel(this);
    m_tableModel_2->setColumnCount(3);
    m_tableModel_2->setRowCount(D);

    m_tableModel_2->setHeaderData(0, Qt::Horizontal, "edge 画像 ID 1");
    m_tableModel_2->setHeaderData(1, Qt::Horizontal, "edge 画像 ID 2");
    m_tableModel_2->setHeaderData(2, Qt::Horizontal, "SSIM");

    for (int n = 0; n < D; ++n) {
        // 全セルにデータを格納
        m_tableModel_2->setData(m_tableModel_2->index(n, 0), data_sel.edge1[n]);
        m_tableModel_2->setData(m_tableModel_2->index(n, 1), data_sel.edge2[n]);
        m_tableModel_2->setData(m_tableModel_2->index(n, 2), data_sel.ssim[n]);
        // 全セルを中央揃え
        for (int i = 0; i < 3; ++i) {
            m_tableModel_2->setData(m_tableModel_2->index(n, i), Qt::AlignCenter, Qt::TextAlignmentRole);
        }
    }

    ui->tableView_2->setModel(m_tableModel_2);

}