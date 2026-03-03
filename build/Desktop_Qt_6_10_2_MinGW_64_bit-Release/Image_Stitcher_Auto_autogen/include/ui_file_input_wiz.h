/********************************************************************************
** Form generated from reading UI file 'file_input_wiz.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FILE_INPUT_WIZ_H
#define UI_FILE_INPUT_WIZ_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include "droparea_wiz.h"
#include "filetablewidget.h"

QT_BEGIN_NAMESPACE

class Ui_FileInputDialog
{
public:
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QComboBox *comboBox;
    QPushButton *sortButton;
    QPushButton *dupButton;
    QPushButton *browseButton;
    DropArea_wiz *dropArea;
    FileTableWidget *tableWidgetFiles;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *FileInputDialog)
    {
        if (FileInputDialog->objectName().isEmpty())
            FileInputDialog->setObjectName("FileInputDialog");
        FileInputDialog->resize(800, 600);
        gridLayout_2 = new QGridLayout(FileInputDialog);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        comboBox = new QComboBox(FileInputDialog);
        comboBox->setObjectName("comboBox");
        comboBox->setMinimumContentsLength(0);

        gridLayout->addWidget(comboBox, 2, 0, 1, 1);

        sortButton = new QPushButton(FileInputDialog);
        sortButton->setObjectName("sortButton");

        gridLayout->addWidget(sortButton, 2, 1, 1, 1);

        dupButton = new QPushButton(FileInputDialog);
        dupButton->setObjectName("dupButton");

        gridLayout->addWidget(dupButton, 3, 0, 1, 2);

        browseButton = new QPushButton(FileInputDialog);
        browseButton->setObjectName("browseButton");

        gridLayout->addWidget(browseButton, 0, 0, 1, 2);

        dropArea = new DropArea_wiz(FileInputDialog);
        dropArea->setObjectName("dropArea");
        dropArea->setMinimumSize(QSize(100, 0));
        dropArea->setFrameShape(QFrame::Shape::StyledPanel);
        dropArea->setFrameShadow(QFrame::Shadow::Raised);

        gridLayout->addWidget(dropArea, 1, 0, 1, 2);

        tableWidgetFiles = new FileTableWidget(FileInputDialog);
        tableWidgetFiles->setObjectName("tableWidgetFiles");

        gridLayout->addWidget(tableWidgetFiles, 0, 2, 4, 1);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);

        buttonBox = new QDialogButtonBox(FileInputDialog);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        gridLayout_2->addWidget(buttonBox, 1, 0, 1, 1);


        retranslateUi(FileInputDialog);

        QMetaObject::connectSlotsByName(FileInputDialog);
    } // setupUi

    void retranslateUi(QDialog *FileInputDialog)
    {
        FileInputDialog->setWindowTitle(QCoreApplication::translate("FileInputDialog", "\343\203\225\343\202\241\343\202\244\343\203\253\345\205\245\345\212\233\343\202\246\343\202\243\343\202\266\343\203\274\343\203\211", nullptr));
        comboBox->setCurrentText(QString());
        sortButton->setText(QCoreApplication::translate("FileInputDialog", "\344\270\246\343\201\271\346\233\277\343\201\210", nullptr));
        dupButton->setText(QCoreApplication::translate("FileInputDialog", "\351\207\215\350\244\207\343\202\222\345\211\212\351\231\244", nullptr));
        browseButton->setText(QCoreApplication::translate("FileInputDialog", "\343\203\225\343\202\241\343\202\244\343\203\253\343\202\222\351\201\270\346\212\236...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FileInputDialog: public Ui_FileInputDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FILE_INPUT_WIZ_H
