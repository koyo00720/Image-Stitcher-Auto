#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QSplashScreen>
#include <QPixmap>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    qputenv("QT_IMAGEIO_MAXALLOC", QByteArray("0"));
#endif

    QApplication a(argc, argv);

    QCoreApplication::setApplicationName("Image Stitcher Auto");
    QCoreApplication::setApplicationVersion("0.3.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption inputOption(
        QStringList() << "i" << "input",
        "入力ファイルのパスを指定。区切り文字は「;」",
        "files");
    parser.addOption(inputOption);

    QCommandLineOption autoOption("auto", "自動で画像の配列を計算（未実装）");
    parser.addOption(autoOption);

    QCommandLineOption manualOption("manual", "画像配列を指定");
    parser.addOption(manualOption);

    QCommandLineOption overxOption(
        QStringList() << "x" << "over_x",
        "横方向の画像同士重複率を設定, [1-100], デフォルト: 25",
        "%");
    overxOption.setDefaultValue("25");
    parser.addOption(overxOption);

    QCommandLineOption overyOption(
        QStringList() << "y" << "over_y",
        "縦方向の画像同士重複率を設定, [1-100], デフォルト: 25",
        "%");
    overyOption.setDefaultValue("25");
    parser.addOption(overyOption);

    QCommandLineOption overrOption(
        QStringList() << "r" << "over_r",
        "探索範囲を設定, [0-100], デフォルト: 15",
        "±%");
    overrOption.setDefaultValue("15");
    parser.addOption(overrOption);

    QCommandLineOption arrayOption(
        QStringList() << "a" <<"array",
        "画像の配列を指定, [1-8], デフォルト: 8, 1:左上→右, 2: 左上→下, 3: 右上→左, 4: 右上→下, 5: 左下→右, 6: 左下→上, 7: 右下→左, 8: 右下→上",
        "int");
    arrayOption.setDefaultValue("8");
    parser.addOption(arrayOption);

    QCommandLineOption arrayhOption(
        QStringList() <<"x_num",
        "横方向の画像枚数を指定, [0-画像枚数], デフォルト: 0, 0:自動, 1-:枚数",
        "int");
    arrayhOption.setDefaultValue("0");
    parser.addOption(arrayhOption);

    QCommandLineOption arrayvOption(
        QStringList() << "y_num",
        "縦方向の画像枚数を指定, [0-画像枚数], デフォルト: 0, 0:自動, 1-:枚数",
        "int");
    arrayvOption.setDefaultValue("0");
    parser.addOption(arrayvOption);

    QCommandLineOption zigOption(
        QStringList() << "z" << "zig",
        "ジグザグ配列にするか指定, [0-1], デフォルト: 1, 0: 一方向, 1: ジグザグ",
        "int");
    zigOption.setDefaultValue("1");
    parser.addOption(zigOption);

    QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        "結合画像の出力先パス",
        "file");
    parser.addOption(outputOption);

    QCommandLineOption closeOption("close", "計算結果が良好な場合、画像出力後にウインドウを閉じる");
    parser.addOption(closeOption);

    parser.process(a);

    QSplashScreen splash(QPixmap(":/splash.png"));
    splash.show();
    a.processEvents();

    MainWindow w;
    w.resize(1300, 800);

    w.show();

    QTimer::singleShot(100, &w, [&w, &parser, &splash, inputOption]() {
        if (parser.isSet(inputOption)) {
            QString raw = parser.value(inputOption);
            QStringList files = raw.split(';', Qt::SkipEmptyParts);
            for (QString &f : files) {
                f = f.trimmed();
            }
            w.File_input_UI();
            w.File_input_check(files);
        }
        splash.finish(&w);
    });

    if (parser.isSet(manualOption)) {
        // 入力チェック
        bool ok = false;
        int o_x = parser.value(overxOption).toInt(&ok);
        if (!ok || o_x < 1 || o_x > 100) {
            o_x = 25;
        }
        ok = false;
        int o_y = parser.value(overyOption).toInt(&ok);
        if (!ok || o_y < 1 || o_y > 100) {
            o_y = 25;
        }
        ok = false;
        int o_r = parser.value(overrOption).toInt(&ok);
        if (!ok || o_r < 0 || o_r > 100) {
            o_r = 15;
        }
        ok = false;
        int ar = parser.value(arrayOption).toInt(&ok);
        if (!ok || ar < 1 || ar > 8) {
            ar = 8;
        }
        ok = false;
        int arh = parser.value(arrayhOption).toInt(&ok);
        if (!ok || arh < 0 || arh > 8) {
            arh = 0;
        }
        ok = false;
        int arv = parser.value(arrayvOption).toInt(&ok);
        if (!ok || arv < 0 || arv > 8) {
            arv = 0;
        }
        ok = false;
        int zig = parser.value(zigOption).toInt(&ok);
        if (!ok || zig < 0 || zig > 1) {
            zig = 1;
        }

        bool closeTF = parser.isSet(closeOption);

        QObject::connect(&w, &MainWindow::fileInputFinished, &w,
                         [&w, &splash, o_x, o_y, o_r, ar, arh, arv, zig, closeTF]() {
                             w.set_over_value(o_x, o_y, o_r);
                             w.set_array_value(ar, arh, arv);
                             w.set_zigzag_value(zig);
                             w.set_close_value(closeTF);
                             w.run_manual();
                         });

        QObject::connect(&w, &MainWindow::calcFinished, &w,
                         [&w]() {
                             w.cli_make_image();
                         });

        if (parser.isSet(outputOption)) {
            QString out_p = parser.value(outputOption);
            QObject::connect(&w, &MainWindow::makeimageFinished, &w,
                             [&w, &out_p]() {
                                 w.cli_exp_image(out_p);
                             });
        }
    }
    return a.exec();
}
