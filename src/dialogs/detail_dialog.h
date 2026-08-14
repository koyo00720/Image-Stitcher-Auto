#ifndef DETAIL_DIALOG_H
#define DETAIL_DIALOG_H

#include <QDialog>

class QTableView;
class QDialogButtonBox;
class QAbstractItemModel;

class Detail_Dialog : public QDialog {
    Q_OBJECT
public:
    explicit Detail_Dialog(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model); // 返り値不要、表示用にセットするだけ

private:
    QTableView* table = nullptr;
    QDialogButtonBox* buttons = nullptr;
};

#endif // DETAIL_DIALOG_H
