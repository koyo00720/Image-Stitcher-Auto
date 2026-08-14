#include "least_squares_settings.h"
#include "ui_least_squares_settings.h"

least_squares_settings::least_squares_settings(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::least_squares_settings)
{
    ui->setupUi(this);
    setWindowTitle("最小二乗法最適化設定");
}

least_squares_settings::~least_squares_settings()
{
    delete ui;
}

void least_squares_settings::setValues(double regressionThreshold,
                                       double relativeThreshold,
                                       double absoluteThreshold,
                                       double maxPairErrorForRelative)
{
    ui->doubleSpinBox->setValue(regressionThreshold);
    ui->doubleSpinBox_2->setValue(relativeThreshold);
    ui->doubleSpinBox_3->setValue(absoluteThreshold);
    ui->doubleSpinBox_4->setValue(maxPairErrorForRelative);
}

std::array<double, 4> least_squares_settings::getValues() const
{
    return {
        ui->doubleSpinBox->value(),
        ui->doubleSpinBox_2->value(),
        ui->doubleSpinBox_3->value(),
        ui->doubleSpinBox_4->value()
    };
}
