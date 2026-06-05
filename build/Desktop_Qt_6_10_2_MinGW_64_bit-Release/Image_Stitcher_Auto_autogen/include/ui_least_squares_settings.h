/********************************************************************************
** Form generated from reading UI file 'least_squares_settings.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LEAST_SQUARES_SETTINGS_H
#define UI_LEAST_SQUARES_SETTINGS_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>

QT_BEGIN_NAMESPACE

class Ui_least_squares_settings
{
public:
    QGridLayout *gridLayout;
    QGroupBox *groupBox;
    QGridLayout *gridLayout_2;
    QLabel *label;
    QDoubleSpinBox *doubleSpinBox;
    QLabel *label_2;
    QDoubleSpinBox *doubleSpinBox_2;
    QLabel *label_3;
    QDoubleSpinBox *doubleSpinBox_3;
    QLabel *label_4;
    QDoubleSpinBox *doubleSpinBox_4;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *least_squares_settings)
    {
        if (least_squares_settings->objectName().isEmpty())
            least_squares_settings->setObjectName("least_squares_settings");
        least_squares_settings->resize(430, 190);
        gridLayout = new QGridLayout(least_squares_settings);
        gridLayout->setObjectName("gridLayout");
        groupBox = new QGroupBox(least_squares_settings);
        groupBox->setObjectName("groupBox");
        gridLayout_2 = new QGridLayout(groupBox);
        gridLayout_2->setObjectName("gridLayout_2");
        label = new QLabel(groupBox);
        label->setObjectName("label");

        gridLayout_2->addWidget(label, 0, 0, 1, 1);

        doubleSpinBox = new QDoubleSpinBox(groupBox);
        doubleSpinBox->setObjectName("doubleSpinBox");
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setMinimum(0.000000000000000);
        doubleSpinBox->setMaximum(1.000000000000000);
        doubleSpinBox->setSingleStep(0.010000000000000);
        doubleSpinBox->setValue(0.300000000000000);

        gridLayout_2->addWidget(doubleSpinBox, 0, 1, 1, 1);

        label_2 = new QLabel(groupBox);
        label_2->setObjectName("label_2");

        gridLayout_2->addWidget(label_2, 1, 0, 1, 1);

        doubleSpinBox_2 = new QDoubleSpinBox(groupBox);
        doubleSpinBox_2->setObjectName("doubleSpinBox_2");
        doubleSpinBox_2->setDecimals(2);
        doubleSpinBox_2->setMinimum(0.000000000000000);
        doubleSpinBox_2->setMaximum(100.000000000000000);
        doubleSpinBox_2->setSingleStep(0.100000000000000);
        doubleSpinBox_2->setValue(2.500000000000000);

        gridLayout_2->addWidget(doubleSpinBox_2, 1, 1, 1, 1);

        label_3 = new QLabel(groupBox);
        label_3->setObjectName("label_3");

        gridLayout_2->addWidget(label_3, 2, 0, 1, 1);

        doubleSpinBox_3 = new QDoubleSpinBox(groupBox);
        doubleSpinBox_3->setObjectName("doubleSpinBox_3");
        doubleSpinBox_3->setDecimals(2);
        doubleSpinBox_3->setMinimum(0.000000000000000);
        doubleSpinBox_3->setMaximum(1000.000000000000000);
        doubleSpinBox_3->setSingleStep(0.100000000000000);
        doubleSpinBox_3->setValue(3.500000000000000);

        gridLayout_2->addWidget(doubleSpinBox_3, 2, 1, 1, 1);

        label_4 = new QLabel(groupBox);
        label_4->setObjectName("label_4");

        gridLayout_2->addWidget(label_4, 3, 0, 1, 1);

        doubleSpinBox_4 = new QDoubleSpinBox(groupBox);
        doubleSpinBox_4->setObjectName("doubleSpinBox_4");
        doubleSpinBox_4->setDecimals(2);
        doubleSpinBox_4->setMinimum(0.000000000000000);
        doubleSpinBox_4->setMaximum(1000.000000000000000);
        doubleSpinBox_4->setSingleStep(0.100000000000000);
        doubleSpinBox_4->setValue(0.950000000000000);

        gridLayout_2->addWidget(doubleSpinBox_4, 3, 1, 1, 1);


        gridLayout->addWidget(groupBox, 0, 0, 1, 1);

        buttonBox = new QDialogButtonBox(least_squares_settings);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        gridLayout->addWidget(buttonBox, 1, 0, 1, 1);


        retranslateUi(least_squares_settings);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, least_squares_settings, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, least_squares_settings, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(least_squares_settings);
    } // setupUi

    void retranslateUi(QDialog *least_squares_settings)
    {
        least_squares_settings->setWindowTitle(QCoreApplication::translate("least_squares_settings", "Dialog", nullptr));
        groupBox->setTitle(QString());
        label->setText(QCoreApplication::translate("least_squares_settings", "\347\233\270\351\226\242\343\202\271\343\202\263\343\202\242\343\201\227\343\201\215\343\201\204\345\200\244", nullptr));
        label_2->setText(QCoreApplication::translate("least_squares_settings", "\347\233\270\345\257\276\345\244\226\343\202\214\345\200\244\343\201\227\343\201\215\343\201\204\345\200\244", nullptr));
        label_3->setText(QCoreApplication::translate("least_squares_settings", "\347\265\266\345\257\276\345\244\226\343\202\214\345\200\244\343\201\227\343\201\215\343\201\204\345\200\244", nullptr));
        doubleSpinBox_3->setSuffix(QCoreApplication::translate("least_squares_settings", " pix", nullptr));
        label_4->setText(QCoreApplication::translate("least_squares_settings", "\347\233\270\345\257\276\345\210\244\345\256\232\343\201\256\346\234\200\345\260\217\346\256\213\345\267\256", nullptr));
        doubleSpinBox_4->setSuffix(QCoreApplication::translate("least_squares_settings", " pix", nullptr));
    } // retranslateUi

};

namespace Ui {
    class least_squares_settings: public Ui_least_squares_settings {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LEAST_SQUARES_SETTINGS_H
