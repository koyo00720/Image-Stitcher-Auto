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
    QCoreApplication::setApplicationVersion("1.0.0");

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

    QCommandLineOption pa0Option(
        QStringList() << "part",
        "局所最適化を行うか, [0-1], デフォルト: 0, 0: しない, 1: する",
        "int");
    pa0Option.setDefaultValue("0");
    parser.addOption(pa0Option);

    QCommandLineOption pa1Option(
        QStringList() << "part_num",
        "局所最適化の画像枚数, [4-10], デフォルト: 6",
        "int");
    pa1Option.setDefaultValue("6");
    parser.addOption(pa1Option);

    QCommandLineOption pa2Option(
        QStringList() << "part_pix",
        "局所最適化の探索範囲半径, [1-10], デフォルト: 2",
        "int");
    pa2Option.setDefaultValue("2");
    parser.addOption(pa2Option);

    QCommandLineOption pa3Option(
        QStringList() << "part_itr",
        "局所最適化の最大計算回数, [20-100000], デフォルト: 5000",
        "int");
    pa3Option.setDefaultValue("5000");
    parser.addOption(pa3Option);

    QCommandLineOption pa4Option(
        QStringList() << "part_loop",
        "局所最適化の最大ループ回数, [1-20], デフォルト: 4",
        "int");
    pa4Option.setDefaultValue("4");
    parser.addOption(pa4Option);

    QCommandLineOption al0Option(
        QStringList() << "all",
        "全体最適化を行うか, [0-1], デフォルト: 1, 0: しない, 1: する",
        "int");
    al0Option.setDefaultValue("1");
    parser.addOption(al0Option);

    QCommandLineOption al2Option(
        QStringList() << "all_pix",
        "局所最適化の探索範囲半径, [1-10], デフォルト: 2",
        "int");
    al2Option.setDefaultValue("2");
    parser.addOption(al2Option);

    QCommandLineOption al3Option(
        QStringList() << "all_itr",
        "局所最適化の最大計算回数, [20-100000], デフォルト: 10000",
        "int");
    al3Option.setDefaultValue("10000");
    parser.addOption(al3Option);

    QCommandLineOption al4Option(
        QStringList() << "all_loop",
        "局所最適化の最大ループ回数, [1-50], デフォルト: 10",
        "int");
    al4Option.setDefaultValue("10");
    parser.addOption(al4Option);

    QCommandLineOption calcOption(
        QStringList() << "calc",
        "どこまで計算を行うか, [0-2], デフォルト: 0, 0: しない, 1: 位相相関法まで, 2: 最適化まで",
        "int");
    calcOption.setDefaultValue("0");
    parser.addOption(calcOption);

    QCommandLineOption miOption("make_image", "画像を作成");
    parser.addOption(miOption);

    QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        "結合画像の出力先パス",
        "file");
    parser.addOption(outputOption);

    QCommandLineOption exiOption("export", "画像をエクスポート");
    parser.addOption(exiOption);

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
        } else {
            w.File_input_dummy();
        }
        splash.finish(&w);
    });

    // 入力チェック
    int manu = 1;
    if (parser.isSet(autoOption)) {
        manu = 0;
    }
    if (parser.isSet(manualOption)) {
        manu = 1;
    }
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
    ok = false;
    int pa0 = parser.value(pa0Option).toInt(&ok);
    if (!ok || pa0 < 0 || pa0 > 1) {
        pa0 = 0;
    }
    ok = false;
    int pa1 = parser.value(pa1Option).toInt(&ok);
    if (!ok || pa1 <= 0) {
        pa1 = 6;
    }
    ok = false;
    int pa2 = parser.value(pa2Option).toInt(&ok);
    if (!ok || pa2 <= 0) {
        pa2 = 2;
    }
    ok = false;
    int pa3 = parser.value(pa3Option).toInt(&ok);
    if (!ok || pa3 <= 0) {
        pa3 = 5000;
    }
    ok = false;
    int pa4 = parser.value(pa4Option).toInt(&ok);
    if (!ok || pa4 <= 0) {
        pa4 = 4;
    }
    ok = false;
    int al0 = parser.value(al0Option).toInt(&ok);
    if (!ok || al0 < 0 || al0 > 1) {
        al0 = 1;
    }
    ok = false;
    int al2 = parser.value(al2Option).toInt(&ok);
    if (!ok || al2 <= 0) {
        al2 = 2;
    }
    ok = false;
    int al3 = parser.value(al3Option).toInt(&ok);
    if (!ok || al3 <= 0) {
        al3 = 10000;
    }
    ok = false;
    int al4 = parser.value(al4Option).toInt(&ok);
    if (!ok || al4 <= 0) {
        al4 = 10;
    }
    ok = false;
    int ca = parser.value(calcOption).toInt(&ok);
    if (!ok || ca < 0 || ca > 2 || !parser.isSet(inputOption)) {
        ca = 0;
    }
    bool mi = parser.isSet(miOption);
    QString out_p;
    if (parser.isSet(outputOption)) {
        out_p = parser.value(outputOption);
    }
    bool expi = parser.isSet(exiOption);
    bool closeTF = parser.isSet(closeOption);

    QObject::connect(&w, &MainWindow::fileInputFinished, &w,
                     [&w,&splash,manu,o_x,o_y,o_r,ar,arh,arv,zig,pa0,pa1,pa2,pa3,pa4,al0,al2,al3,al4,ca,&out_p,closeTF]() {
                        w.set_over_value(o_x, o_y, o_r);
                        w.set_array_value(ar, arh, arv);
                        w.set_zigzag_value(zig);
                        w.set_output_path(out_p);
                        w.set_close_value(closeTF);
                        w.set_opti_value(pa0,pa1,pa2,pa3,pa4,al0,al2,al3,al4);
                        w.run_manual(ca);
                     });

    QObject::connect(&w, &MainWindow::calcFinished, &w,
                     [&w,mi]() {
                    if (mi) {
                        w.cli_make_image();
                    }
                     });

    if (expi) {
        QObject::connect(&w, &MainWindow::makeimageFinished, &w,
                         [&w]() {
                             w.cli_exp_image();
                         });
    }

    return a.exec();
}
