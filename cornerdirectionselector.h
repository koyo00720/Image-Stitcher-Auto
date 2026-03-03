#ifndef CORNERDIRECTIONSELECTOR_H
#define CORNERDIRECTIONSELECTOR_H

#include <QWidget>
#include <QPoint>
#include "autospinbox.h"
#include <QSlider>
#include <QLabel>

class CornerDirectionSelector : public QWidget
{
    Q_OBJECT
public:
    explicit CornerDirectionSelector(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override { return QSize(grid_size+right_arrow_width+marginR+marginL, grid_size+down_arrow_width+marginT+marginD); }
    QSize sizeHint() const override { return QSize(grid_size+right_arrow_width+marginR+marginL, grid_size+down_arrow_width+marginT+marginD); }

    // 外部からintを受け取り、UIを描画
    void setUI(int);
    // ui->cornerSelector->setUI(1); のようにして実行

    // 外部からintを受け取り、行数や列数のmax値の参考にする。
    void setMax(int);

    // 自動で行数を設定する
    void setRauto();

    // 自動で列数を設定する
    void setCauto();

    // 外部からintを受け取り、行数を指定する。
    void setRows(int);

    // 外部からintを受け取り、列数を指定する。
    void setCols(int);

    // 行数を取得する
    int getStatus() {return state_UI;}

    // 行数を取得する
    int getRows() {return r_num;}

    // 列数を取得する
    int getCols() {return c_num;}

signals:
    // 画像のレイアウトを通知
    void stateChanged(int); // 0=未選択, 1..8=有効状態
    // 行数を通知
    void r_Changed(int); // 0=Auto
    // 列数を通知
    void c_Changed(int); // 0=Auto

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    static constexpr int kRows = 4;
    //static constexpr int kCols = 4;

    const int grid_size = 96;
    // const int grid_hight = 120;
    const int marginT = 1;
    const int marginL = 4;
    const int marginD = 11;
    const int marginR = 10;
    const int spacing = 4;
    const int right_arrow_width = 130;
    const int down_arrow_width = 70;

    bool m_updating = false; // 相互シグナルの制御用
    void onRowsChanged(int);
    void onColsChanged(int);

    QLabel *labelR = nullptr;
    AutoSpinBox *spinR = nullptr;
    QSlider *sliderR = nullptr;

    QLabel *labelC = nullptr;
    AutoSpinBox *spinC = nullptr;
    QSlider *sliderC = nullptr;

    bool m_First = false;
    bool m_Second = false;
    QPoint m_firstCell;   // (col, row)
    QPoint m_secondCell;  // (col, row)

    QRect cellRect(int row, int col) const;
    QPoint hitTestCell(const QPoint& pos) const;

    int photo_num = 0; // 扱っている画像の枚数
    int r_num = -1; // 行数
    int c_num = -1; // 列数

    void enable_UI(int, bool);
    int state_UI = -1; // UIの状態を保持
};




#endif // CORNERDIRECTIONSELECTOR_H
