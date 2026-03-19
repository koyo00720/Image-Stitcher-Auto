/********************************************************************************
** Form generated from reading UI file 'opti_settings.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OPTI_SETTINGS_H
#define UI_OPTI_SETTINGS_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>

QT_BEGIN_NAMESPACE

class Ui_opti_settings
{
public:
    QGridLayout *gridLayout;
    QGroupBox *groupBox_2;
    QGridLayout *gridLayout_3;
    QSlider *horizontalSlider_6;
    QSlider *horizontalSlider_5;
    QLabel *label_5;
    QSpinBox *spinBox_5;
    QLabel *label_6;
    QSpinBox *spinBox_6;
    QLabel *label_7;
    QSlider *horizontalSlider_7;
    QSpinBox *spinBox_7;
    QCheckBox *checkBox;
    QGroupBox *groupBox;
    QGridLayout *gridLayout_2;
    QSpinBox *spinBox_2;
    QSlider *horizontalSlider;
    QLabel *label_3;
    QSpinBox *spinBox_3;
    QLabel *label_2;
    QLabel *label;
    QSlider *horizontalSlider_3;
    QSpinBox *spinBox;
    QSlider *horizontalSlider_2;
    QLabel *label_4;
    QSlider *horizontalSlider_4;
    QSpinBox *spinBox_4;
    QDialogButtonBox *buttonBox;
    QCheckBox *checkBox_2;

    void setupUi(QDialog *opti_settings)
    {
        if (opti_settings->objectName().isEmpty())
            opti_settings->setObjectName("opti_settings");
        opti_settings->resize(520, 367);
        gridLayout = new QGridLayout(opti_settings);
        gridLayout->setObjectName("gridLayout");
        groupBox_2 = new QGroupBox(opti_settings);
        groupBox_2->setObjectName("groupBox_2");
        gridLayout_3 = new QGridLayout(groupBox_2);
        gridLayout_3->setObjectName("gridLayout_3");
        horizontalSlider_6 = new QSlider(groupBox_2);
        horizontalSlider_6->setObjectName("horizontalSlider_6");
        horizontalSlider_6->setMinimum(20);
        horizontalSlider_6->setMaximum(100000);
        horizontalSlider_6->setValue(10000);
        horizontalSlider_6->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_3->addWidget(horizontalSlider_6, 1, 1, 1, 1);

        horizontalSlider_5 = new QSlider(groupBox_2);
        horizontalSlider_5->setObjectName("horizontalSlider_5");
        horizontalSlider_5->setMinimum(1);
        horizontalSlider_5->setMaximum(10);
        horizontalSlider_5->setValue(3);
        horizontalSlider_5->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_3->addWidget(horizontalSlider_5, 0, 1, 1, 1);

        label_5 = new QLabel(groupBox_2);
        label_5->setObjectName("label_5");

        gridLayout_3->addWidget(label_5, 0, 0, 1, 1);

        spinBox_5 = new QSpinBox(groupBox_2);
        spinBox_5->setObjectName("spinBox_5");
        spinBox_5->setMinimum(1);
        spinBox_5->setMaximum(10);
        spinBox_5->setValue(3);

        gridLayout_3->addWidget(spinBox_5, 0, 2, 1, 1);

        label_6 = new QLabel(groupBox_2);
        label_6->setObjectName("label_6");

        gridLayout_3->addWidget(label_6, 1, 0, 1, 1);

        spinBox_6 = new QSpinBox(groupBox_2);
        spinBox_6->setObjectName("spinBox_6");
        spinBox_6->setMinimum(20);
        spinBox_6->setMaximum(100000);
        spinBox_6->setValue(10000);

        gridLayout_3->addWidget(spinBox_6, 1, 2, 1, 1);

        label_7 = new QLabel(groupBox_2);
        label_7->setObjectName("label_7");

        gridLayout_3->addWidget(label_7, 2, 0, 1, 1);

        horizontalSlider_7 = new QSlider(groupBox_2);
        horizontalSlider_7->setObjectName("horizontalSlider_7");
        horizontalSlider_7->setMinimum(1);
        horizontalSlider_7->setMaximum(50);
        horizontalSlider_7->setValue(10);
        horizontalSlider_7->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_3->addWidget(horizontalSlider_7, 2, 1, 1, 1);

        spinBox_7 = new QSpinBox(groupBox_2);
        spinBox_7->setObjectName("spinBox_7");
        spinBox_7->setMinimum(1);
        spinBox_7->setMaximum(50);
        spinBox_7->setValue(10);

        gridLayout_3->addWidget(spinBox_7, 2, 2, 1, 1);


        gridLayout->addWidget(groupBox_2, 3, 0, 1, 1);

        checkBox = new QCheckBox(opti_settings);
        checkBox->setObjectName("checkBox");
        checkBox->setChecked(true);

        gridLayout->addWidget(checkBox, 0, 0, 1, 1);

        groupBox = new QGroupBox(opti_settings);
        groupBox->setObjectName("groupBox");
        gridLayout_2 = new QGridLayout(groupBox);
        gridLayout_2->setObjectName("gridLayout_2");
        spinBox_2 = new QSpinBox(groupBox);
        spinBox_2->setObjectName("spinBox_2");
        spinBox_2->setMinimum(20);
        spinBox_2->setMaximum(100000);
        spinBox_2->setValue(5000);

        gridLayout_2->addWidget(spinBox_2, 2, 2, 1, 1);

        horizontalSlider = new QSlider(groupBox);
        horizontalSlider->setObjectName("horizontalSlider");
        horizontalSlider->setMinimum(4);
        horizontalSlider->setMaximum(10);
        horizontalSlider->setValue(6);
        horizontalSlider->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_2->addWidget(horizontalSlider, 0, 1, 1, 1);

        label_3 = new QLabel(groupBox);
        label_3->setObjectName("label_3");

        gridLayout_2->addWidget(label_3, 3, 0, 1, 1);

        spinBox_3 = new QSpinBox(groupBox);
        spinBox_3->setObjectName("spinBox_3");
        spinBox_3->setMinimum(1);
        spinBox_3->setMaximum(20);
        spinBox_3->setValue(5);

        gridLayout_2->addWidget(spinBox_3, 3, 2, 1, 1);

        label_2 = new QLabel(groupBox);
        label_2->setObjectName("label_2");

        gridLayout_2->addWidget(label_2, 2, 0, 1, 1);

        label = new QLabel(groupBox);
        label->setObjectName("label");

        gridLayout_2->addWidget(label, 0, 0, 1, 1);

        horizontalSlider_3 = new QSlider(groupBox);
        horizontalSlider_3->setObjectName("horizontalSlider_3");
        horizontalSlider_3->setMinimum(1);
        horizontalSlider_3->setMaximum(20);
        horizontalSlider_3->setValue(5);
        horizontalSlider_3->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_2->addWidget(horizontalSlider_3, 3, 1, 1, 1);

        spinBox = new QSpinBox(groupBox);
        spinBox->setObjectName("spinBox");
        spinBox->setMinimum(4);
        spinBox->setMaximum(10);
        spinBox->setValue(6);

        gridLayout_2->addWidget(spinBox, 0, 2, 1, 1);

        horizontalSlider_2 = new QSlider(groupBox);
        horizontalSlider_2->setObjectName("horizontalSlider_2");
        horizontalSlider_2->setMinimum(20);
        horizontalSlider_2->setMaximum(100000);
        horizontalSlider_2->setValue(5000);
        horizontalSlider_2->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_2->addWidget(horizontalSlider_2, 2, 1, 1, 1);

        label_4 = new QLabel(groupBox);
        label_4->setObjectName("label_4");

        gridLayout_2->addWidget(label_4, 1, 0, 1, 1);

        horizontalSlider_4 = new QSlider(groupBox);
        horizontalSlider_4->setObjectName("horizontalSlider_4");
        horizontalSlider_4->setMinimum(1);
        horizontalSlider_4->setMaximum(10);
        horizontalSlider_4->setValue(3);
        horizontalSlider_4->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_2->addWidget(horizontalSlider_4, 1, 1, 1, 1);

        spinBox_4 = new QSpinBox(groupBox);
        spinBox_4->setObjectName("spinBox_4");
        spinBox_4->setMinimum(1);
        spinBox_4->setMaximum(10);
        spinBox_4->setValue(3);

        gridLayout_2->addWidget(spinBox_4, 1, 2, 1, 1);


        gridLayout->addWidget(groupBox, 1, 0, 1, 1);

        buttonBox = new QDialogButtonBox(opti_settings);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        gridLayout->addWidget(buttonBox, 4, 0, 1, 1);

        checkBox_2 = new QCheckBox(opti_settings);
        checkBox_2->setObjectName("checkBox_2");
        checkBox_2->setChecked(true);

        gridLayout->addWidget(checkBox_2, 2, 0, 1, 1);


        retranslateUi(opti_settings);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, opti_settings, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, opti_settings, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(opti_settings);
    } // setupUi

    void retranslateUi(QDialog *opti_settings)
    {
        opti_settings->setWindowTitle(QCoreApplication::translate("opti_settings", "Dialog", nullptr));
        groupBox_2->setTitle(QString());
        label_5->setText(QCoreApplication::translate("opti_settings", "\346\216\242\347\264\242\347\257\204\345\233\262\345\215\212\345\276\204\343\200\200\343\200\200\343\200\200", nullptr));
        spinBox_5->setSuffix(QCoreApplication::translate("opti_settings", " pix", nullptr));
        label_6->setText(QCoreApplication::translate("opti_settings", "\346\234\200\345\244\247\350\250\210\347\256\227\345\233\236\346\225\260", nullptr));
        label_7->setText(QCoreApplication::translate("opti_settings", "\346\234\200\345\244\247\343\203\253\343\203\274\343\203\227\345\233\236\346\225\260", nullptr));
        checkBox->setText(QCoreApplication::translate("opti_settings", "\345\261\200\346\211\200\343\203\253\343\203\274\343\203\227\346\234\200\351\201\251\345\214\226", nullptr));
        groupBox->setTitle(QString());
        label_3->setText(QCoreApplication::translate("opti_settings", "\346\234\200\345\244\247\343\203\253\343\203\274\343\203\227\345\233\236\346\225\260", nullptr));
        label_2->setText(QCoreApplication::translate("opti_settings", "\346\234\200\345\244\247\350\250\210\347\256\227\345\233\236\346\225\260", nullptr));
        label->setText(QCoreApplication::translate("opti_settings", "\350\250\210\347\256\227\345\257\276\350\261\241\343\201\256\347\224\273\345\203\217\346\236\232\346\225\260", nullptr));
        label_4->setText(QCoreApplication::translate("opti_settings", "\346\216\242\347\264\242\347\257\204\345\233\262\345\215\212\345\276\204", nullptr));
        spinBox_4->setSuffix(QCoreApplication::translate("opti_settings", " pix", nullptr));
        checkBox_2->setText(QCoreApplication::translate("opti_settings", "\345\205\250\344\275\223\346\234\200\351\201\251\345\214\226", nullptr));
    } // retranslateUi

};

namespace Ui {
    class opti_settings: public Ui_opti_settings {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OPTI_SETTINGS_H
