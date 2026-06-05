/********************************************************************************
** Form generated from reading UI file 'merge_settings.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MERGE_SETTINGS_H
#define UI_MERGE_SETTINGS_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>

QT_BEGIN_NAMESPACE

class Ui_merge_settings
{
public:
    QGridLayout *gridLayout;
    QGroupBox *groupBox;
    QGridLayout *gridLayout_2;
    QLabel *label;
    QComboBox *comboBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *merge_settings)
    {
        if (merge_settings->objectName().isEmpty())
            merge_settings->setObjectName("merge_settings");
        merge_settings->resize(430, 105);
        gridLayout = new QGridLayout(merge_settings);
        gridLayout->setObjectName("gridLayout");
        groupBox = new QGroupBox(merge_settings);
        groupBox->setObjectName("groupBox");
        gridLayout_2 = new QGridLayout(groupBox);
        gridLayout_2->setObjectName("gridLayout_2");
        label = new QLabel(groupBox);
        label->setObjectName("label");

        gridLayout_2->addWidget(label, 0, 0, 1, 1);

        comboBox = new QComboBox(groupBox);
        comboBox->setObjectName("comboBox");

        gridLayout_2->addWidget(comboBox, 0, 1, 1, 1);


        gridLayout->addWidget(groupBox, 0, 0, 1, 1);

        buttonBox = new QDialogButtonBox(merge_settings);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        gridLayout->addWidget(buttonBox, 1, 0, 1, 1);


        retranslateUi(merge_settings);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, merge_settings, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, merge_settings, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(merge_settings);
    } // setupUi

    void retranslateUi(QDialog *merge_settings)
    {
        merge_settings->setWindowTitle(QCoreApplication::translate("merge_settings", "Dialog", nullptr));
        groupBox->setTitle(QString());
        label->setText(QCoreApplication::translate("merge_settings", "\347\265\220\345\220\210\343\203\242\343\203\274\343\203\211", nullptr));
    } // retranslateUi

};

namespace Ui {
    class merge_settings: public Ui_merge_settings {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MERGE_SETTINGS_H
