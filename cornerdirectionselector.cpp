#include "cornerdirectionselector.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPen>
#include <QFont>
#include <QSignalBlocker>
#include <QMessageBox>
#include <cmath>

void CornerDirectionSelector::onRowsChanged(int rows)
{
    if (m_updating) return;      // 更新中なら何もしない
    m_updating = true;

    // 4つ全部の signal を止める（ここが重要）
    QSignalBlocker b1(sliderR);
    QSignalBlocker b2(spinR);
    QSignalBlocker b3(sliderC);
    QSignalBlocker b4(spinC);

    // Rows を UI に反映
    spinR->setValue(rows);
    sliderR->setValue(rows);

    // Cols を計算して UI に反映
    int cols = 0;
    if (rows == 0) {
        cols = 0;
    } else {
        cols = (photo_num + rows - 1) / rows; // 切り上げ
    }
    spinC->setValue(cols);
    sliderC->setValue(cols);

    m_updating = false;
    r_num = rows;
    c_num = cols;
    emit r_Changed(rows);
}

void CornerDirectionSelector::onColsChanged(int cols)
{
    if (m_updating) return;
    m_updating = true;

    QSignalBlocker b1(sliderR);
    QSignalBlocker b2(spinR);
    QSignalBlocker b3(sliderC);
    QSignalBlocker b4(spinC);

    spinC->setValue(cols);
    sliderC->setValue(cols);

    int rows = 0;
    if (cols == 0) {
        rows = 0;
    } else {
        rows = (photo_num + cols - 1) / cols; // 切り上げ
    }
    spinR->setValue(rows);
    sliderR->setValue(rows);

    m_updating = false;
    r_num = rows;
    c_num = cols;
    emit c_Changed(cols);
}

CornerDirectionSelector::CornerDirectionSelector(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);

    // 行数の制御
    labelR = new QLabel("行数", this);
    labelR->setGeometry(marginL+marginR+grid_size+22, marginT+grid_size/2 - 18, 40, 22);

    spinR = new AutoSpinBox(this);
    spinR->setRange(0, photo_num);
    spinR->setGeometry(marginL+marginR+grid_size+50, marginT+grid_size/2 - 18, 80, 22);

    sliderR = new QSlider(Qt::Horizontal, this);
    sliderR->setRange(0, photo_num);
    sliderR->setGeometry(marginL+marginR+grid_size+20, marginT+grid_size/2 + 10, 110, 18);

    connect(sliderR, &QSlider::valueChanged, this, [this](int v) { onRowsChanged(v); });
    connect(spinR, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { onRowsChanged(v); });

    // 列数の制御
    labelC = new QLabel("列数", this);
    labelC->setGeometry(marginL+22 - 15, marginT+marginD+grid_size+40 - 18, 40, 22);

    spinC = new AutoSpinBox(this);
    spinC->setRange(0, photo_num);
    spinC->setGeometry(marginL+50 - 15, marginT+marginD+grid_size+40 - 18, 80, 22);

    sliderC = new QSlider(Qt::Horizontal, this);
    sliderC->setRange(0, photo_num);
    sliderC->setGeometry(marginL+20 - 15, marginT+marginD+grid_size+40 + 10, 110, 18);

    connect(sliderC, &QSlider::valueChanged, this, [this](int v) { onColsChanged(v); });
    connect(spinC, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) { onColsChanged(v); });

    onRowsChanged(0);

    enable_UI(0, true);
}


// 行、列番号を元に、そのgridの位置とサイズを計算する
QRect CornerDirectionSelector::cellRect(int row, int col) const
{
    const int availableW = grid_size - (kRows - 1) * spacing;
    const int cellSize = availableW / kRows;

    //const int gridW = cellSize * kRows + spacing * (kRows - 1);
    //const int gridH = cellSize * kRows + spacing * (kRows - 1);

    //const int startX = (grid_size - gridW) / 2;
    //const int startY = (grid_size - gridH) / 2;

    const int x = marginL + col * (cellSize + spacing);
    const int y = marginT + row * (cellSize + spacing);
    return QRect(x, y, cellSize, cellSize);
}

// クリックしたマウス座標をgrid位置へ変換
QPoint CornerDirectionSelector::hitTestCell(const QPoint& pos) const
{
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kRows; ++c) {
            if (cellRect(r, c).contains(pos)) {
                return QPoint(c, r);
            }
        }
    }
    return QPoint(-1, -1);
}

// UI描画
void CornerDirectionSelector::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kRows; ++c) {
            const QPoint cell(c, r);
            const QRect rc = cellRect(r, c);

            QColor border = palette().color(QPalette::WindowText);
            qreal penWidth = 1;

            const bool isFirst  = (m_First  && cell == m_firstCell);
            const bool isSecond = (m_Second && cell == m_secondCell);

            // 選択済みセルは枠色＋太さで強調（内部は塗らない）
            if (isFirst) {
                border = QColor(40, 90, 180);
                penWidth = 3.0;
            }
            if (isSecond) {
                border = QColor(40, 130, 55);
                penWidth = 3.0;
            }

            p.setPen(QPen(border, penWidth));
            p.setBrush(Qt::NoBrush);              // ← 塗りつぶしなし（透過）
            p.drawRoundedRect(rc, 4, 4);

            // 1 / 2 の数字表示
            // 塗りつぶしが無いので、白だと見えにくい場合は枠色に合わせる方が良い
            QFont fnt = p.font();
            //fnt.setBold(true);
            p.setFont(fnt);

            QColor co = this->palette().color(QPalette::Text);

            if (isFirst) {
                //p.setPen(QColor(40, 90, 180));    // 白ではなく青系
                p.setPen(co);
                p.drawText(rc, Qt::AlignCenter, "1");
            }
            if (isSecond) {
                //p.setPen(QColor(40, 130, 55));    // 白ではなく緑系
                p.setPen(co);
                p.drawText(rc, Qt::AlignCenter, "2");
            }
        }
    }

    // 下の矢印描画
    // 線色
    QPen pen(palette().color(QPalette::WindowText));
    pen.setWidth(1);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    p.setBrush(palette().color(QPalette::WindowText));

    // 寸法線の位置
    const int y = marginT + grid_size + marginD + 2;
    const int x1 = marginL;
    const int x2 = marginL + grid_size;

    // 補助線長さ
    const int dl = 9;

    // 矢印形状
    const int arx = 8;
    const int ary = 3;

    // 両端の縦の補助線（画像の短い縦線）
    p.drawLine(x1, y - dl, x1, y + dl);
    p.drawLine(x2, y - dl, x2, y + dl);

    // 中央の水平線（矢印の内側まで）
    const int arrowLen = 4;  // 矢印の長さ
    p.drawLine(x1 + arrowLen, y, x2 - arrowLen, y);

    // 左矢印（内向き）
    QPoint tipL(x1 + arrowLen, y);  // 矢印の先端
    p.drawLine(tipL, QPoint(tipL.x() + arx, tipL.y() - ary));
    p.drawLine(tipL, QPoint(tipL.x() + arx, tipL.y() + ary));

    // 右矢印（内向き）
    QPoint tipR(x2 - arrowLen, y);  // 矢印の先端
    p.drawLine(tipR, QPoint(tipR.x() - arx, tipR.y() - ary));
    p.drawLine(tipR, QPoint(tipR.x() - arx, tipR.y() + ary));

    // 右の矢印描画
    // 寸法線の位置
    const int x = marginL + grid_size + marginR + 2;
    const int y1 = marginT;
    const int y2 = marginT + grid_size;

    // 両端の縦の補助線（画像の短い縦線）
    p.drawLine(x - dl, y1, x + dl, y1);
    p.drawLine(x - dl, y2, x + dl, y2);

    // 中央の水平線（矢印の内側まで）
    p.drawLine(x, y1 + arrowLen, x , y2 - arrowLen);

    // 左矢印（内向き）
    QPoint tipT(x, y1 + arrowLen);  // 矢印の先端
    p.drawLine(tipT, QPoint(tipT.x() - ary, tipT.y() + arx));
    p.drawLine(tipT, QPoint(tipT.x() + ary, tipT.y() + arx));

    // 右矢印（内向き）
    QPoint tipD(x, y2 - arrowLen);  // 矢印の先端
    p.drawLine(tipD, QPoint(tipD.x() - ary, tipD.y() - arx));
    p.drawLine(tipD, QPoint(tipD.x() + ary, tipD.y() - arx));

}

// マウスで押された場合に実行
void CornerDirectionSelector::mousePressEvent(QMouseEvent* event)
{
    // 左クリック以外に反応しない
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint cell = hitTestCell(event->pos());
    if (cell.x() < 0 || cell.y() < 0) { // grid以外の場所が押された場合
        /*
        m_First = false;
        m_Second = false;
        emit stateChanged(0);
        */
        return;
    }
    // 1: 左上→右    2: 左上→下
    // 3: 右上→左    4: 右上→下
    // 5: 左下→右    6: 左下→上
    // 7: 右下→左    8: 右下→上

    if (cell == QPoint(1, 0)) { // 左上→右
        enable_UI(1, true);
    } else if (cell == QPoint(0, 0) || cell == QPoint(0, 1)) { // 左上→下
        enable_UI(2, true);
    } else if (cell == QPoint(kRows - 2, 0)) { // 右上→左
        enable_UI(3, true);
    } else if (cell == QPoint(kRows - 1, 0) || cell == QPoint(kRows - 1, 1)) { // 右上→下
        enable_UI(4, true);
    } else if (cell == QPoint(1, kRows - 1)) { // 左下→右
        enable_UI(5, true);
    } else if (cell == QPoint(0, kRows - 1) || cell == QPoint(0, kRows - 2)) { // 左下→上
        enable_UI(6, true);
    } else if (cell == QPoint(kRows - 2, kRows - 1)) { // 右下→左
        enable_UI(7, true);
    } else if (cell == QPoint(kRows - 1, kRows - 1) || cell == QPoint(kRows - 1, kRows - 2)) { // 右下→上
        enable_UI(8, true);
    } else {
        return;
    }
    update(); // UI更新
}

// 外部からint値を受け取り、UIを描画する
void CornerDirectionSelector::setUI(int state_int)
{
    if (state_int < -1 || state_int > 8) return;

    enable_UI(state_int, true);
    update(); // UI更新
}

void CornerDirectionSelector::enable_UI(int i, bool check)
{
    switch (i) {
        case 1:
            state_UI = 1;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(0, 0);
            m_secondCell = QPoint(1, 0);
            this->labelR->setEnabled(false);
            this->spinR->setEnabled(false);
            this->sliderR->setEnabled(false);
            this->labelC->setEnabled(true);
            this->spinC->setEnabled(true);
            this->sliderC->setEnabled(true);
            if (check) {emit stateChanged(1);}
            break;
        case 2:
            state_UI = 2;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(0, 0);
            m_secondCell = QPoint(0, 1);
            this->labelR->setEnabled(true);
            this->spinR->setEnabled(true);
            this->sliderR->setEnabled(true);
            this->labelC->setEnabled(false);
            this->spinC->setEnabled(false);
            this->sliderC->setEnabled(false);
            if (check) {emit stateChanged(2);}
            break;
        case 3:
            state_UI = 3;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(kRows - 1, 0);
            m_secondCell = QPoint(kRows - 2, 0);
            this->labelR->setEnabled(false);
            this->spinR->setEnabled(false);
            this->sliderR->setEnabled(false);
            this->labelC->setEnabled(true);
            this->spinC->setEnabled(true);
            this->sliderC->setEnabled(true);
            if (check) {emit stateChanged(3);}
            break;
        case 4:
            state_UI = 4;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(kRows - 1, 0);
            m_secondCell = QPoint(kRows - 1, 1);
            this->labelR->setEnabled(true);
            this->spinR->setEnabled(true);
            this->sliderR->setEnabled(true);
            this->labelC->setEnabled(false);
            this->spinC->setEnabled(false);
            this->sliderC->setEnabled(false);
            if (check) {emit stateChanged(4);}
            break;
        case 5:
            state_UI = 5;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(0, kRows - 1);
            m_secondCell = QPoint(1, kRows - 1);
            this->labelR->setEnabled(false);
            this->spinR->setEnabled(false);
            this->sliderR->setEnabled(false);
            this->labelC->setEnabled(true);
            this->spinC->setEnabled(true);
            this->sliderC->setEnabled(true);
            if (check) {emit stateChanged(5);}
            break;
        case 6:
            state_UI = 6;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(0, kRows - 1);
            m_secondCell = QPoint(0, kRows - 2);
            this->labelR->setEnabled(true);
            this->spinR->setEnabled(true);
            this->sliderR->setEnabled(true);
            this->labelC->setEnabled(false);
            this->spinC->setEnabled(false);
            this->sliderC->setEnabled(false);
            if (check) {emit stateChanged(6);}
            break;
        case 7:
            state_UI = 7;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(kRows - 1, kRows - 1);
            m_secondCell = QPoint(kRows - 2, kRows - 1);
            this->labelR->setEnabled(false);
            this->spinR->setEnabled(false);
            this->sliderR->setEnabled(false);
            this->labelC->setEnabled(true);
            this->spinC->setEnabled(true);
            this->sliderC->setEnabled(true);
            if (check) {emit stateChanged(7);}
            break;
        case 8:
            state_UI = 8;
            m_First = true;
            m_Second = true;
            m_firstCell = QPoint(kRows - 1, kRows - 1);
            m_secondCell = QPoint(kRows - 1, kRows - 2);
            this->labelR->setEnabled(true);
            this->spinR->setEnabled(true);
            this->sliderR->setEnabled(true);
            this->labelC->setEnabled(false);
            this->spinC->setEnabled(false);
            this->sliderC->setEnabled(false);
            if (check) {emit stateChanged(8);}
            break;
        default:
            state_UI = -1;
            m_First = false;
            m_Second = false;
            this->labelR->setEnabled(false);
            this->spinR->setEnabled(false);
            this->sliderR->setEnabled(false);
            this->labelC->setEnabled(false);
            this->spinC->setEnabled(false);
            this->sliderC->setEnabled(false);
            if (check) {emit stateChanged(0);}
            break;
    }
}

void CornerDirectionSelector::setMax(int i)
{
    spinR->setRange(0, i);
    sliderR->setRange(0, i);
    spinC->setRange(0, i);
    sliderC->setRange(0, i);
    photo_num = i;
    //if (state_UI == 0) // 自動配列モード
    if (r_num != 0 || c_num != 0) {
        if (state_UI == 1 || state_UI == 3 || state_UI == 5 || state_UI == 7) {
            onColsChanged(c_num);
        } else if (state_UI == 2 || state_UI == 4 || state_UI == 6 || state_UI == 8) {
            onRowsChanged(r_num);
        }
    } else if (state_UI == -1) { // 初回
        setRauto();
    } else if ((r_num == 0 && c_num == 0)) { // state_UI == 0の場合
        if (state_UI == 1 || state_UI == 3 || state_UI == 5 || state_UI == 7) {
            /*
            int r = (int)std::sqrt(photo_num);
            int q = (photo_num + r - 1) / r;
            onColsChanged(q);
*/
            onColsChanged(0);
        } else if (state_UI == 2 || state_UI == 4 || state_UI == 6 || state_UI == 8) {
            /*
            int r = (int)std::sqrt(photo_num);
            int q = (photo_num + r - 1) / r;
            onRowsChanged(q);
*/
            onRowsChanged(0);
        }
    }
}

void CornerDirectionSelector::setRauto()
{
    enable_UI(8, false);
    update();
    onRowsChanged(0);
    /*
    int r = (int)std::sqrt(photo_num);
    int q = (photo_num + r - 1) / r;
    onRowsChanged(q);
    */
}

void CornerDirectionSelector::setCauto()
{
    enable_UI(1, false);
    update();
    int r = (int)std::sqrt(photo_num);
    int q = (photo_num + r - 1) / r;
    onColsChanged(q);
}

void CornerDirectionSelector::setRows(int i)
{
    if (i > photo_num) {
        QMessageBox::warning(
            this,
            tr("警告"),
            "指定された行数は画像枚数を超えています。"
            );
    } else if (i < 0) {
        QMessageBox::warning(
            this,
            tr("警告"),
            "0以上の行数を指定してください。（0=Auto）"
            );
    } else {
        if (state_UI == -1) {
            QMessageBox::warning(
                this,
                tr("警告"),
                "setUIを初めに実行してください。"
                );
        } else if (state_UI == 1 || state_UI == 3 || state_UI == 5 || state_UI == 7) {
            QMessageBox::warning(
                this,
                tr("警告"),
                "setColsを使って設定してください。"
                );
        } else {
            onRowsChanged(i);
        }
    }
}

void CornerDirectionSelector::setCols(int i)
{
    if (i > photo_num) {
        QMessageBox::warning(
            this,
            tr("警告"),
            "指定された列数は画像枚数を超えています。"
            );
    } else if (i < 0) {
        QMessageBox::warning(
            this,
            tr("警告"),
            "0以上の列数を指定してください。（0=Auto）"
            );
    } else {
        if (state_UI == -1) {
            QMessageBox::warning(
                this,
                tr("警告"),
                "setUIを初めに実行してください。"
                );
        } else if (state_UI == 2 || state_UI == 4 || state_UI == 6 || state_UI == 8) {
            QMessageBox::warning(
                this,
                tr("警告"),
                "setRowsを使って設定してください。"
                );
        } else {
            onColsChanged(i);
        }
    }
}
