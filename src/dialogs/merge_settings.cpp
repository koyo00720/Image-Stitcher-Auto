#include "merge_settings.h"
#include "ui_merge_settings.h"

#include <QEvent>

#include <algorithm>

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
    const int mode = std::max(0, ui->comboBox->currentIndex());
    ui->retranslateUi(this);
    setWindowTitle(tr("画像結合設定"));
    ui->comboBox->clear();
    ui->comboBox->addItem(tr("境界距離重み（L2）"));
    ui->comboBox->addItem(tr("重なり領域ごとに高フォーカス画像を使用"));
    ui->comboBox->addItem(tr("Tenengradフォーカススタッキング"));
    ui->comboBox->setCurrentIndex(std::min(mode, ui->comboBox->count() - 1));
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
