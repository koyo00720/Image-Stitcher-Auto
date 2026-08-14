#include "merge_settings.h"
#include "ui_merge_settings.h"

merge_settings::merge_settings(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::merge_settings)
{
    ui->setupUi(this);
    setWindowTitle("画像結合設定");

    ui->comboBox->clear();
    ui->comboBox->addItem("境界距離重み（L2）");
    ui->comboBox->addItem("重なり領域ごとに高フォーカス画像を使用");
    ui->comboBox->addItem("Tenengradフォーカススタッキング");
}

merge_settings::~merge_settings()
{
    delete ui;
}

void merge_settings::setMode(int mode)
{
    if (mode < 0 || mode >= ui->comboBox->count()) {
        mode = 0;
    }
    ui->comboBox->setCurrentIndex(mode);
}

int merge_settings::getMode() const
{
    return ui->comboBox->currentIndex();
}
