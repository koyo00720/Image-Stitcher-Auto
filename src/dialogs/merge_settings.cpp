#include "merge_settings.h"
#include "ui_merge_settings.h"

#include <QEvent>

merge_settings::merge_settings(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::merge_settings)
{
    ui->setupUi(this);
    retranslateUi();
}

merge_settings::~merge_settings()
{
    delete ui;
}

void merge_settings::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void merge_settings::retranslateUi()
{
    ui->retranslateUi(this);
    setWindowTitle(tr("画像結合設定"));
    ui->groupBox->setTitle(tr("結合モード"));
    ui->distanceL2Radio->setText(tr("境界距離重み（L2）"));
    ui->focusRegionRadio->setText(tr("重なり領域ごとに高フォーカス画像を使用"));
    ui->focusStackRadio->setText(tr("Tenengradフォーカススタッキング"));
    ui->imageOrderRadio->setText(tr("画像番号順に上書き"));
}

void merge_settings::setMode(int mode)
{
    switch (mode) {
    case 1: ui->focusRegionRadio->setChecked(true); break;
    case 2: ui->focusStackRadio->setChecked(true); break;
    case 3: ui->imageOrderRadio->setChecked(true); break;
    case 0:
    default: ui->distanceL2Radio->setChecked(true); break;
    }
}

int merge_settings::getMode() const
{
    if (ui->focusRegionRadio->isChecked()) return 1;
    if (ui->focusStackRadio->isChecked()) return 2;
    if (ui->imageOrderRadio->isChecked()) return 3;
    return 0;
}
