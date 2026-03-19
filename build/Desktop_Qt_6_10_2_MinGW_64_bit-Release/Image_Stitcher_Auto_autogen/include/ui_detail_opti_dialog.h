/********************************************************************************
** Form generated from reading UI file 'detail_opti_dialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DETAIL_OPTI_DIALOG_H
#define UI_DETAIL_OPTI_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_detail_opti_dialog
{
public:
    QVBoxLayout *verticalLayout;
    QPushButton *pushButton;
    QWidget *splitterHost;
    QGroupBox *groupBox;
    QGridLayout *gridLayout;
    QTableView *tableView;
    QGroupBox *groupBox_2;
    QGridLayout *gridLayout_2;
    QTableView *tableView_2;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *detail_opti_dialog)
    {
        if (detail_opti_dialog->objectName().isEmpty())
            detail_opti_dialog->setObjectName("detail_opti_dialog");
        detail_opti_dialog->resize(1000, 560);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(detail_opti_dialog->sizePolicy().hasHeightForWidth());
        detail_opti_dialog->setSizePolicy(sizePolicy);
        verticalLayout = new QVBoxLayout(detail_opti_dialog);
        verticalLayout->setObjectName("verticalLayout");
        pushButton = new QPushButton(detail_opti_dialog);
        pushButton->setObjectName("pushButton");

        verticalLayout->addWidget(pushButton);

        splitterHost = new QWidget(detail_opti_dialog);
        splitterHost->setObjectName("splitterHost");
        groupBox = new QGroupBox(splitterHost);
        groupBox->setObjectName("groupBox");
        groupBox->setGeometry(QRect(10, 10, 982, 235));
        sizePolicy.setHeightForWidth(groupBox->sizePolicy().hasHeightForWidth());
        groupBox->setSizePolicy(sizePolicy);
        gridLayout = new QGridLayout(groupBox);
        gridLayout->setObjectName("gridLayout");
        tableView = new QTableView(groupBox);
        tableView->setObjectName("tableView");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(tableView->sizePolicy().hasHeightForWidth());
        tableView->setSizePolicy(sizePolicy1);

        gridLayout->addWidget(tableView, 0, 0, 1, 1);

        groupBox_2 = new QGroupBox(splitterHost);
        groupBox_2->setObjectName("groupBox_2");
        groupBox_2->setGeometry(QRect(0, 240, 982, 466));
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(groupBox_2->sizePolicy().hasHeightForWidth());
        groupBox_2->setSizePolicy(sizePolicy2);
        gridLayout_2 = new QGridLayout(groupBox_2);
        gridLayout_2->setObjectName("gridLayout_2");
        tableView_2 = new QTableView(groupBox_2);
        tableView_2->setObjectName("tableView_2");

        gridLayout_2->addWidget(tableView_2, 0, 0, 1, 1);


        verticalLayout->addWidget(splitterHost);

        buttonBox = new QDialogButtonBox(detail_opti_dialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setOrientation(Qt::Orientation::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(detail_opti_dialog);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, detail_opti_dialog, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, detail_opti_dialog, qOverload<>(&QDialog::reject));

        QMetaObject::connectSlotsByName(detail_opti_dialog);
    } // setupUi

    void retranslateUi(QDialog *detail_opti_dialog)
    {
        detail_opti_dialog->setWindowTitle(QCoreApplication::translate("detail_opti_dialog", "Dialog", nullptr));
        pushButton->setText(QCoreApplication::translate("detail_opti_dialog", "\343\203\207\343\203\274\343\202\277\343\202\222\346\233\264\346\226\260", nullptr));
        groupBox->setTitle(QCoreApplication::translate("detail_opti_dialog", "\346\234\200\351\201\251\345\214\226\350\250\210\347\256\227\347\265\220\346\236\234\343\201\256\346\246\202\350\246\201", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("detail_opti_dialog", "\350\251\263\347\264\260", nullptr));
    } // retranslateUi

};

namespace Ui {
    class detail_opti_dialog: public Ui_detail_opti_dialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DETAIL_OPTI_DIALOG_H
