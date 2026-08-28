#ifndef DETAIL_OPTI_DIALOG_H
#define DETAIL_OPTI_DIALOG_H

#include <QDialog>
#include <vector>
#include <QStandardItemModel>
#include <QModelIndex>

namespace Ui {
class detail_opti_dialog;
}

struct out_detail {
    bool PaAll;
    int start;
    int end;
    int itr;
    int loop;
    bool shuusoku;
    int lowSSIM_num;
    double minSSIM;
    double energy;
};

struct out_log {
    std::vector<double> ssim;
    std::vector<int> edge1;
    std::vector<int> edge2;
};

struct one_line {
    out_detail abst;
    out_log detail;
};

class detail_opti_dialog : public QDialog
{
    Q_OBJECT

public:
    explicit detail_opti_dialog(QWidget *parent = nullptr);
    ~detail_opti_dialog();

    //void setData(bool,int,int,int,int,bool,int,double,double);
    void setData(out_detail,out_log);
    std::vector<one_line> data() const;
    void setAllData(const std::vector<one_line>& data);
    void clearData();

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void refreshUi();
    void onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    Ui::detail_opti_dialog *ui;

    // 全データを保持
    std::vector<one_line> all_data;

    // Table1
    QStandardItemModel* m_tableModel = nullptr;
};

#endif // DETAIL_OPTI_DIALOG_H
