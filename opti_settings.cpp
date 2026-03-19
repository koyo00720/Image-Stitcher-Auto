#include "opti_settings.h"
#include "ui_opti_settings.h"

opti_settings::opti_settings(QWidget *parent) : QDialog(parent), ui(new Ui::opti_settings)
{
    ui->setupUi(this);
    setWindowTitle("最適化設定");

    connect(ui->horizontalSlider, &QSlider::valueChanged,
            ui->spinBox, &QSpinBox::setValue);
    connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider, &QSlider::setValue);

    connect(ui->horizontalSlider_2, &QSlider::valueChanged,
            ui->spinBox_2, &QSpinBox::setValue);
    connect(ui->spinBox_2, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_2, &QSlider::setValue);

    connect(ui->horizontalSlider_3, &QSlider::valueChanged,
            ui->spinBox_3, &QSpinBox::setValue);
    connect(ui->spinBox_3, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_3, &QSlider::setValue);

    connect(ui->horizontalSlider_4, &QSlider::valueChanged,
            ui->spinBox_4, &QSpinBox::setValue);
    connect(ui->spinBox_4, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_4, &QSlider::setValue);

    connect(ui->horizontalSlider_5, &QSlider::valueChanged,
            ui->spinBox_5, &QSpinBox::setValue);
    connect(ui->spinBox_5, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_5, &QSlider::setValue);

    connect(ui->horizontalSlider_6, &QSlider::valueChanged,
            ui->spinBox_6, &QSpinBox::setValue);
    connect(ui->spinBox_6, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_6, &QSlider::setValue);

    connect(ui->horizontalSlider_7, &QSlider::valueChanged,
            ui->spinBox_7, &QSpinBox::setValue);
    connect(ui->spinBox_7, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_7, &QSlider::setValue);

    connect(ui->checkBox, &QCheckBox::toggled,this, &opti_settings::lock1);
    connect(ui->checkBox_2, &QCheckBox::toggled,this, &opti_settings::lock2);
}

opti_settings::~opti_settings()
{
    delete ui;
}

void opti_settings::lock1(bool checked) {
    if (checked) {
        ui->groupBox->setEnabled(true);
    } else {
        ui->groupBox->setEnabled(false);
    }
}

void opti_settings::lock2(bool checked) {
    if (checked) {
        ui->groupBox_2->setEnabled(true);
    } else {
        ui->groupBox_2->setEnabled(false);
    }
}

void opti_settings::setValues(int pa_num,int pa_radi,int pa_opti,int pa_itr,int all_radi,int all_opti,int all_itr,bool pa_TF,bool all_TF)
{
    if (pa_num < ui->spinBox->minimum()) {
        ui->spinBox->setMinimum(pa_num);
        ui->horizontalSlider->setMinimum(pa_num);
    } else if (pa_num > ui->spinBox->maximum()) {
        ui->spinBox->setMaximum(pa_num);
        ui->horizontalSlider->setMaximum(pa_num);
    }
    ui->spinBox->setValue(pa_num);

    if (pa_radi < ui->spinBox_4->minimum()) {
        ui->spinBox_4->setMinimum(pa_radi);
        ui->horizontalSlider_4->setMinimum(pa_radi);
    } else if (pa_radi > ui->spinBox_4->maximum()) {
        ui->spinBox_4->setMaximum(pa_radi);
        ui->horizontalSlider_4->setMaximum(pa_radi);
    }
    ui->spinBox_4->setValue(pa_radi);

    if (pa_opti < ui->spinBox_2->minimum()) {
        ui->spinBox_2->setMinimum(pa_opti);
        ui->horizontalSlider_2->setMinimum(pa_opti);
    } else if (pa_opti > ui->spinBox_2->maximum()) {
        ui->spinBox_2->setMaximum(pa_opti);
        ui->horizontalSlider_2->setMaximum(pa_opti);
    }
    ui->spinBox_2->setValue(pa_opti);

    if (pa_itr < ui->spinBox_3->minimum()) {
        ui->spinBox_3->setMinimum(pa_itr);
        ui->horizontalSlider_3->setMinimum(pa_itr);
    } else if (pa_itr > ui->spinBox_3->maximum()) {
        ui->spinBox_3->setMaximum(pa_itr);
        ui->horizontalSlider_3->setMaximum(pa_itr);
    }
    ui->spinBox_3->setValue(pa_itr);

    if (all_radi < ui->spinBox_5->minimum()) {
        ui->spinBox_5->setMinimum(all_radi);
        ui->horizontalSlider_5->setMinimum(all_radi);
    } else if (all_radi > ui->spinBox_5->maximum()) {
        ui->spinBox_5->setMaximum(all_radi);
        ui->horizontalSlider_5->setMaximum(all_radi);
    }
    ui->spinBox_5->setValue(all_radi);

    if (all_opti < ui->spinBox_6->minimum()) {
        ui->spinBox_6->setMinimum(all_opti);
        ui->horizontalSlider_6->setMinimum(all_opti);
    } else if (all_opti > ui->spinBox_6->maximum()) {
        ui->spinBox_6->setMaximum(all_opti);
        ui->horizontalSlider_6->setMaximum(all_opti);
    }
    ui->spinBox_6->setValue(all_opti);

    if (all_itr < ui->spinBox_7->minimum()) {
        ui->spinBox_7->setMinimum(all_itr);
        ui->horizontalSlider_7->setMinimum(all_itr);
    } else if (all_itr > ui->spinBox_7->maximum()) {
        ui->spinBox_7->setMaximum(all_itr);
        ui->horizontalSlider_7->setMaximum(all_itr);
    }
    ui->spinBox_7->setValue(all_itr);

    ui->checkBox->setChecked(pa_TF);
    ui->checkBox_2->setChecked(all_TF);
}

std::vector<int> opti_settings::getValues()
{
    std::vector<int> r;
    r.resize(7);
    r[0] = ui->spinBox->value();
    r[1] = ui->spinBox_4->value();
    r[2] = ui->spinBox_2->value();
    r[3] = ui->spinBox_3->value();
    r[4] = ui->spinBox_5->value();
    r[5] = ui->spinBox_6->value();
    r[6] = ui->spinBox_7->value();
    return r;
}

std::vector<bool> opti_settings::getTFs()
{
    std::vector<bool> r;
    r.resize(2);
    r[0] = ui->checkBox->isChecked();
    r[1] = ui->checkBox_2->isChecked();
    return r;
}