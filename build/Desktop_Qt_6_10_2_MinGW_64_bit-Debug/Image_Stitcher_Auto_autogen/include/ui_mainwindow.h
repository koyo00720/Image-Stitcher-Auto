/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>
#include "campus.h"
#include "cornerdirectionselector.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QGridLayout *gridLayout_2;
    maincampus *graphicsView;
    QGridLayout *GridLayout;
    QGroupBox *groupBox;
    QGridLayout *gridLayout_3;
    QRadioButton *radioButton_6;
    QRadioButton *radioButton_5;
    QGroupBox *groupBox_2;
    QGridLayout *gridLayout_5;
    QRadioButton *radioButton_3;
    QRadioButton *radioButton_4;
    QGroupBox *groupBox_6;
    QGridLayout *gridLayout_7;
    QPushButton *pushButton_Calc1;
    QLabel *label_6;
    QPushButton *pushButton;
    QPushButton *pushButton_2;
    QLabel *label_5;
    QLabel *label_4;
    QPushButton *pushButton_3;
    QGroupBox *groupBox_3;
    QGridLayout *gridLayout;
    CornerDirectionSelector *cornerSelector;
    QGroupBox *groupBox_5;
    QGridLayout *gridLayout_6;
    QSlider *sliderOpacity1;
    QSpinBox *spinOpacity1;
    QComboBox *comboBox;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_8;
    QLabel *label_9;
    QPushButton *pushButton_4;
    QPushButton *pushButton_1;
    QLabel *label_7;
    QGroupBox *groupBox_4;
    QGridLayout *gridLayout_4;
    QLabel *label_11;
    QSpinBox *spinBox_2;
    QSlider *horizontalSlider_2;
    QSlider *horizontalSlider;
    QLabel *label_10;
    QSpinBox *spinBox;
    QSpinBox *spinBox_3;
    QSlider *horizontalSlider_3;
    QLabel *label_3;
    QCheckBox *checkBox;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1200, 800);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(MainWindow->sizePolicy().hasHeightForWidth());
        MainWindow->setSizePolicy(sizePolicy);
        MainWindow->setBaseSize(QSize(0, 0));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        gridLayout_2 = new QGridLayout(centralwidget);
        gridLayout_2->setObjectName("gridLayout_2");
        graphicsView = new maincampus(centralwidget);
        graphicsView->setObjectName("graphicsView");

        gridLayout_2->addWidget(graphicsView, 0, 2, 1, 1);

        GridLayout = new QGridLayout();
        GridLayout->setObjectName("GridLayout");
        groupBox = new QGroupBox(centralwidget);
        groupBox->setObjectName("groupBox");
        groupBox->setStyleSheet(QString::fromUtf8(""));
        gridLayout_3 = new QGridLayout(groupBox);
        gridLayout_3->setObjectName("gridLayout_3");
        radioButton_6 = new QRadioButton(groupBox);
        radioButton_6->setObjectName("radioButton_6");

        gridLayout_3->addWidget(radioButton_6, 0, 1, 1, 1);

        radioButton_5 = new QRadioButton(groupBox);
        radioButton_5->setObjectName("radioButton_5");

        gridLayout_3->addWidget(radioButton_5, 0, 0, 1, 1);


        GridLayout->addWidget(groupBox, 4, 0, 1, 3);

        groupBox_2 = new QGroupBox(centralwidget);
        groupBox_2->setObjectName("groupBox_2");
        gridLayout_5 = new QGridLayout(groupBox_2);
        gridLayout_5->setObjectName("gridLayout_5");
        radioButton_3 = new QRadioButton(groupBox_2);
        radioButton_3->setObjectName("radioButton_3");

        gridLayout_5->addWidget(radioButton_3, 1, 0, 1, 1);

        radioButton_4 = new QRadioButton(groupBox_2);
        radioButton_4->setObjectName("radioButton_4");

        gridLayout_5->addWidget(radioButton_4, 1, 1, 1, 1);


        GridLayout->addWidget(groupBox_2, 7, 0, 1, 3);

        groupBox_6 = new QGroupBox(centralwidget);
        groupBox_6->setObjectName("groupBox_6");
        gridLayout_7 = new QGridLayout(groupBox_6);
        gridLayout_7->setObjectName("gridLayout_7");
        pushButton_Calc1 = new QPushButton(groupBox_6);
        pushButton_Calc1->setObjectName("pushButton_Calc1");

        gridLayout_7->addWidget(pushButton_Calc1, 0, 0, 1, 3);

        label_6 = new QLabel(groupBox_6);
        label_6->setObjectName("label_6");
        label_6->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_7->addWidget(label_6, 3, 2, 1, 1);

        pushButton = new QPushButton(groupBox_6);
        pushButton->setObjectName("pushButton");

        gridLayout_7->addWidget(pushButton, 1, 2, 1, 1);

        pushButton_2 = new QPushButton(groupBox_6);
        pushButton_2->setObjectName("pushButton_2");

        gridLayout_7->addWidget(pushButton_2, 3, 0, 1, 2);

        label_5 = new QLabel(groupBox_6);
        label_5->setObjectName("label_5");
        label_5->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_7->addWidget(label_5, 1, 1, 1, 1);

        label_4 = new QLabel(groupBox_6);
        label_4->setObjectName("label_4");
        label_4->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_7->addWidget(label_4, 1, 0, 1, 1);

        pushButton_3 = new QPushButton(groupBox_6);
        pushButton_3->setObjectName("pushButton_3");

        gridLayout_7->addWidget(pushButton_3, 2, 0, 1, 3);


        GridLayout->addWidget(groupBox_6, 13, 0, 1, 3);

        groupBox_3 = new QGroupBox(centralwidget);
        groupBox_3->setObjectName("groupBox_3");
        gridLayout = new QGridLayout(groupBox_3);
        gridLayout->setObjectName("gridLayout");
        cornerSelector = new CornerDirectionSelector(groupBox_3);
        cornerSelector->setObjectName("cornerSelector");

        gridLayout->addWidget(cornerSelector, 0, 0, 1, 1);


        GridLayout->addWidget(groupBox_3, 6, 0, 1, 3);

        groupBox_5 = new QGroupBox(centralwidget);
        groupBox_5->setObjectName("groupBox_5");
        gridLayout_6 = new QGridLayout(groupBox_5);
        gridLayout_6->setObjectName("gridLayout_6");
        sliderOpacity1 = new QSlider(groupBox_5);
        sliderOpacity1->setObjectName("sliderOpacity1");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(sliderOpacity1->sizePolicy().hasHeightForWidth());
        sliderOpacity1->setSizePolicy(sizePolicy1);
        sliderOpacity1->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
        sliderOpacity1->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_6->addWidget(sliderOpacity1, 3, 2, 1, 1);

        spinOpacity1 = new QSpinBox(groupBox_5);
        spinOpacity1->setObjectName("spinOpacity1");

        gridLayout_6->addWidget(spinOpacity1, 3, 3, 1, 1);

        comboBox = new QComboBox(groupBox_5);
        comboBox->setObjectName("comboBox");

        gridLayout_6->addWidget(comboBox, 1, 2, 1, 2);

        label = new QLabel(groupBox_5);
        label->setObjectName("label");
        label->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_6->addWidget(label, 1, 1, 1, 1);

        label_2 = new QLabel(groupBox_5);
        label_2->setObjectName("label_2");
        label_2->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_6->addWidget(label_2, 3, 1, 1, 1);

        label_8 = new QLabel(groupBox_5);
        label_8->setObjectName("label_8");
        label_8->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_6->addWidget(label_8, 2, 1, 1, 1);

        label_9 = new QLabel(groupBox_5);
        label_9->setObjectName("label_9");
        label_9->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_6->addWidget(label_9, 2, 2, 1, 2);


        GridLayout->addWidget(groupBox_5, 11, 0, 1, 3);

        pushButton_4 = new QPushButton(centralwidget);
        pushButton_4->setObjectName("pushButton_4");

        GridLayout->addWidget(pushButton_4, 14, 0, 1, 3);

        pushButton_1 = new QPushButton(centralwidget);
        pushButton_1->setObjectName("pushButton_1");

        GridLayout->addWidget(pushButton_1, 2, 0, 1, 3);

        label_7 = new QLabel(centralwidget);
        label_7->setObjectName("label_7");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(label_7->sizePolicy().hasHeightForWidth());
        label_7->setSizePolicy(sizePolicy2);

        GridLayout->addWidget(label_7, 15, 2, 1, 1);

        groupBox_4 = new QGroupBox(centralwidget);
        groupBox_4->setObjectName("groupBox_4");
        gridLayout_4 = new QGridLayout(groupBox_4);
        gridLayout_4->setObjectName("gridLayout_4");
        label_11 = new QLabel(groupBox_4);
        label_11->setObjectName("label_11");

        gridLayout_4->addWidget(label_11, 1, 0, 1, 1);

        spinBox_2 = new QSpinBox(groupBox_4);
        spinBox_2->setObjectName("spinBox_2");
        spinBox_2->setMinimum(1);
        spinBox_2->setMaximum(100);
        spinBox_2->setValue(25);

        gridLayout_4->addWidget(spinBox_2, 0, 2, 1, 1);

        horizontalSlider_2 = new QSlider(groupBox_4);
        horizontalSlider_2->setObjectName("horizontalSlider_2");
        horizontalSlider_2->setMinimum(1);
        horizontalSlider_2->setMaximum(100);
        horizontalSlider_2->setValue(25);
        horizontalSlider_2->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_4->addWidget(horizontalSlider_2, 1, 1, 1, 1);

        horizontalSlider = new QSlider(groupBox_4);
        horizontalSlider->setObjectName("horizontalSlider");
        horizontalSlider->setMinimum(1);
        horizontalSlider->setMaximum(100);
        horizontalSlider->setValue(25);
        horizontalSlider->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_4->addWidget(horizontalSlider, 0, 1, 1, 1);

        label_10 = new QLabel(groupBox_4);
        label_10->setObjectName("label_10");

        gridLayout_4->addWidget(label_10, 0, 0, 1, 1);

        spinBox = new QSpinBox(groupBox_4);
        spinBox->setObjectName("spinBox");
        spinBox->setMaximum(100);
        spinBox->setSingleStep(1);
        spinBox->setValue(15);

        gridLayout_4->addWidget(spinBox, 2, 2, 1, 1);

        spinBox_3 = new QSpinBox(groupBox_4);
        spinBox_3->setObjectName("spinBox_3");
        spinBox_3->setMinimum(1);
        spinBox_3->setMaximum(100);
        spinBox_3->setValue(25);

        gridLayout_4->addWidget(spinBox_3, 1, 2, 1, 1);

        horizontalSlider_3 = new QSlider(groupBox_4);
        horizontalSlider_3->setObjectName("horizontalSlider_3");
        horizontalSlider_3->setMaximum(100);
        horizontalSlider_3->setSingleStep(1);
        horizontalSlider_3->setOrientation(Qt::Orientation::Horizontal);

        gridLayout_4->addWidget(horizontalSlider_3, 2, 1, 1, 1);

        label_3 = new QLabel(groupBox_4);
        label_3->setObjectName("label_3");
        label_3->setAlignment(Qt::AlignmentFlag::AlignCenter);

        gridLayout_4->addWidget(label_3, 2, 0, 1, 1);


        GridLayout->addWidget(groupBox_4, 5, 0, 1, 3);

        checkBox = new QCheckBox(centralwidget);
        checkBox->setObjectName("checkBox");
        checkBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);

        GridLayout->addWidget(checkBox, 3, 0, 1, 3);


        gridLayout_2->addLayout(GridLayout, 0, 0, 1, 1);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1200, 21));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "Image Stitcher Auto", nullptr));
        groupBox->setTitle(QCoreApplication::translate("MainWindow", "\347\224\273\345\203\217\343\201\256\351\205\215\345\210\227\346\226\271\346\263\225", nullptr));
        radioButton_6->setText(QCoreApplication::translate("MainWindow", "\350\207\252\345\213\225\357\274\210\346\234\252\345\256\237\350\243\205\357\274\211", nullptr));
        radioButton_5->setText(QCoreApplication::translate("MainWindow", "\346\211\213\345\213\225", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("MainWindow", "\346\212\230\343\202\212\350\277\224\343\201\227\346\226\271\346\263\225", nullptr));
        radioButton_3->setText(QCoreApplication::translate("MainWindow", "\342\206\221\342\206\223 \343\202\270\343\202\260\343\202\266\343\202\260", nullptr));
        radioButton_4->setText(QCoreApplication::translate("MainWindow", "\342\206\221\342\206\221 \344\270\200\346\226\271\345\220\221\357\274\210\346\234\252\345\256\214\346\210\220\357\274\211", nullptr));
        groupBox_6->setTitle(QCoreApplication::translate("MainWindow", "\350\250\210\347\256\227", nullptr));
        pushButton_Calc1->setText(QCoreApplication::translate("MainWindow", "\344\275\215\347\275\256\345\220\210\343\202\217\343\201\233\357\274\210\344\275\215\347\233\270\347\233\270\351\226\242\346\263\225\357\274\211", nullptr));
        label_6->setText(QString());
        pushButton->setText(QCoreApplication::translate("MainWindow", "\350\251\263\347\264\260", nullptr));
        pushButton_2->setText(QCoreApplication::translate("MainWindow", "\347\224\273\345\203\217\343\202\222\344\275\234\346\210\220", nullptr));
        label_5->setText(QString());
        label_4->setText(QCoreApplication::translate("MainWindow", "\347\265\220\345\220\210\345\223\201\350\263\252", nullptr));
        pushButton_3->setText(QCoreApplication::translate("MainWindow", "\344\275\215\347\275\256\345\220\210\343\202\217\343\201\233\357\274\210\345\205\250\344\275\223\346\234\200\351\201\251\345\214\226\343\200\201\346\234\252\345\256\237\350\243\205\357\274\211", nullptr));
        groupBox_3->setTitle(QCoreApplication::translate("MainWindow", "\343\203\254\343\202\244\343\202\242\343\202\246\343\203\210", nullptr));
        groupBox_5->setTitle(QCoreApplication::translate("MainWindow", "\343\202\255\343\203\243\343\203\263\343\203\221\343\202\271", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "\350\203\214\346\231\257\350\211\262", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "\351\201\270\346\212\236\347\224\273\345\203\217\343\201\256\351\200\217\346\230\216\345\272\246", nullptr));
        label_8->setText(QCoreApplication::translate("MainWindow", "\351\201\270\346\212\236\344\270\255\343\201\256\347\224\273\345\203\217\347\225\252\345\217\267", nullptr));
        label_9->setText(QString());
        pushButton_4->setText(QCoreApplication::translate("MainWindow", "PNG \343\202\250\343\202\257\343\202\271\343\203\235\343\203\274\343\203\210", nullptr));
        pushButton_1->setText(QCoreApplication::translate("MainWindow", "\345\205\245\345\212\233\347\224\273\345\203\217", nullptr));
        label_7->setText(QString());
        groupBox_4->setTitle(QCoreApplication::translate("MainWindow", "\347\224\273\345\203\217\345\220\214\345\243\253\343\201\256\351\207\215\343\201\252\343\202\212\347\233\256\345\256\211", nullptr));
        label_11->setText(QCoreApplication::translate("MainWindow", "\345\236\202\347\233\264\346\226\271\345\220\221\351\207\215\343\201\252\343\202\212", nullptr));
        spinBox_2->setSuffix(QCoreApplication::translate("MainWindow", " %", nullptr));
        label_10->setText(QCoreApplication::translate("MainWindow", "\346\260\264\345\271\263\346\226\271\345\220\221\351\207\215\343\201\252\343\202\212", nullptr));
        spinBox->setSuffix(QCoreApplication::translate("MainWindow", " %", nullptr));
        spinBox->setPrefix(QCoreApplication::translate("MainWindow", "\302\261 ", nullptr));
        spinBox_3->setSuffix(QCoreApplication::translate("MainWindow", " %", nullptr));
        label_3->setText(QCoreApplication::translate("MainWindow", "\346\216\242\347\264\242\347\257\204\345\233\262", nullptr));
        checkBox->setText(QCoreApplication::translate("MainWindow", "\347\224\273\345\203\217\343\201\256\351\205\215\347\275\256\343\202\222\343\203\255\343\203\203\343\202\257", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
