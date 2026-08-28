#include "mainwindow.h"
#include "app_settings.h"
#include "platform_setup.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QSplashScreen>
#include <QPixmap>

int main(int argc, char *argv[])
{
    image_stitcher::platform::configureEnvironment();

    QApplication a(argc, argv);

    QCoreApplication::setApplicationName("Image Stitcher Auto");
    QCoreApplication::setOrganizationName("Image Stitcher Auto");
    QCoreApplication::setApplicationVersion("1.2.1");
    const ApplicationDefaultSettings& defaults = AppSettings::defaults();
    applyApplicationLanguage(AppSettings::language());
    applyApplicationTheme(AppSettings::theme());

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption inputOption(
        QStringList() << "i" << "input",
        QCoreApplication::translate("main", "入力ファイルのパスを指定。区切り文字は「;」"),
        "files");
    parser.addOption(inputOption);

    /*
    QCommandLineOption autoOption("auto", "自動で画像の配列を計算（未実装）");
    parser.addOption(autoOption);

    QCommandLineOption manualOption("manual", "画像配列を指定");
    parser.addOption(manualOption);
    */
    QCommandLineOption overxOption(
        QStringList() << "x" << "over_x",
        QCoreApplication::translate("main", "横方向の画像同士重複率を設定, [1-100], デフォルト: %1")
            .arg(defaults.alignment.horizontalOverlapPercent),
        "%");
    overxOption.setDefaultValue(QString::number(defaults.alignment.horizontalOverlapPercent));
    parser.addOption(overxOption);

    QCommandLineOption overyOption(
        QStringList() << "y" << "over_y",
        QCoreApplication::translate("main", "縦方向の画像同士重複率を設定, [1-100], デフォルト: %1")
            .arg(defaults.alignment.verticalOverlapPercent),
        "%");
    overyOption.setDefaultValue(QString::number(defaults.alignment.verticalOverlapPercent));
    parser.addOption(overyOption);

    QCommandLineOption overrOption(
        QStringList() << "r" << "over_r",
        QCoreApplication::translate("main", "探索範囲を設定, [0-100], デフォルト: %1")
            .arg(defaults.alignment.searchRangePercent),
        "±%");
    overrOption.setDefaultValue(QString::number(defaults.alignment.searchRangePercent));
    parser.addOption(overrOption);

    QCommandLineOption arrayOption(
        QStringList() << "a" <<"array",
        QCoreApplication::translate("main", "画像の配列を指定, [1-8], デフォルト: %1, 1:左上→右, 2: 左上→下, 3: 右上→左, 4: 右上→下, 5: 左下→右, 6: 左下→上, 7: 右下→左, 8: 右下→上")
            .arg(defaults.arrangement.direction),
        "int");
    arrayOption.setDefaultValue(QString::number(defaults.arrangement.direction));
    parser.addOption(arrayOption);

    QCommandLineOption arrayhOption(
        QStringList() <<"x_num",
        QCoreApplication::translate("main", "横方向の画像枚数を指定, [0-画像枚数], デフォルト: %1, 0:自動, 1-:枚数")
            .arg(defaults.arrangement.horizontalImageCount),
        "int");
    arrayhOption.setDefaultValue(QString::number(defaults.arrangement.horizontalImageCount));
    parser.addOption(arrayhOption);

    QCommandLineOption arrayvOption(
        QStringList() << "y_num",
        QCoreApplication::translate("main", "縦方向の画像枚数を指定, [0-画像枚数], デフォルト: %1, 0:自動, 1-:枚数")
            .arg(defaults.arrangement.verticalImageCount),
        "int");
    arrayvOption.setDefaultValue(QString::number(defaults.arrangement.verticalImageCount));
    parser.addOption(arrayvOption);

    QCommandLineOption zigOption(
        QStringList() << "z" << "zig",
        QCoreApplication::translate("main", "ジグザグ配列にするか指定, [0-1], デフォルト: %1, 0: 一方向, 1: ジグザグ")
            .arg(defaults.arrangement.zigzag ? 1 : 0),
        "int");
    zigOption.setDefaultValue(QString::number(defaults.arrangement.zigzag ? 1 : 0));
    parser.addOption(zigOption);

    QCommandLineOption pa0Option(
        QStringList() << "part",
        QCoreApplication::translate("main", "局所最適化を行うか, [0-1], デフォルト: %1, 0: しない, 1: する")
            .arg(defaults.trwsPami.localEnabled ? 1 : 0),
        "int");
    pa0Option.setDefaultValue(QString::number(defaults.trwsPami.localEnabled ? 1 : 0));
    parser.addOption(pa0Option);

    QCommandLineOption pa1Option(
        QStringList() << "part_num",
        QCoreApplication::translate("main", "局所最適化の画像枚数, [4-100], デフォルト: %1")
            .arg(defaults.trwsPami.localImageCount),
        "int");
    pa1Option.setDefaultValue(QString::number(defaults.trwsPami.localImageCount));
    parser.addOption(pa1Option);

    QCommandLineOption pa2Option(
        QStringList() << "part_pix",
        QCoreApplication::translate("main", "局所最適化の探索範囲半径, [1-10], デフォルト: %1")
            .arg(defaults.trwsPami.localSearchRadius),
        "int");
    pa2Option.setDefaultValue(QString::number(defaults.trwsPami.localSearchRadius));
    parser.addOption(pa2Option);

    QCommandLineOption pa3Option(
        QStringList() << "part_itr",
        QCoreApplication::translate("main", "局所最適化の最大計算回数, [20-100000], デフォルト: %1")
            .arg(defaults.trwsPami.localMaxIterations),
        "int");
    pa3Option.setDefaultValue(QString::number(defaults.trwsPami.localMaxIterations));
    parser.addOption(pa3Option);

    QCommandLineOption pa4Option(
        QStringList() << "part_loop",
        QCoreApplication::translate("main", "局所最適化の最大ループ回数, [1-20], デフォルト: %1")
            .arg(defaults.trwsPami.localMaxLoops),
        "int");
    pa4Option.setDefaultValue(QString::number(defaults.trwsPami.localMaxLoops));
    parser.addOption(pa4Option);

    QCommandLineOption al0Option(
        QStringList() << "all",
        QCoreApplication::translate("main", "全体最適化を行うか, [0-1], デフォルト: %1, 0: しない, 1: する")
            .arg(defaults.trwsPami.globalEnabled ? 1 : 0),
        "int");
    al0Option.setDefaultValue(QString::number(defaults.trwsPami.globalEnabled ? 1 : 0));
    parser.addOption(al0Option);

    QCommandLineOption al2Option(
        QStringList() << "all_pix",
        QCoreApplication::translate("main", "全体最適化の探索範囲半径, [1-10], デフォルト: %1")
            .arg(defaults.trwsPami.globalSearchRadius),
        "int");
    al2Option.setDefaultValue(QString::number(defaults.trwsPami.globalSearchRadius));
    parser.addOption(al2Option);

    QCommandLineOption al3Option(
        QStringList() << "all_itr",
        QCoreApplication::translate("main", "全体最適化の最大計算回数, [20-100000], デフォルト: %1")
            .arg(defaults.trwsPami.globalMaxIterations),
        "int");
    al3Option.setDefaultValue(QString::number(defaults.trwsPami.globalMaxIterations));
    parser.addOption(al3Option);

    QCommandLineOption al4Option(
        QStringList() << "all_loop",
        QCoreApplication::translate("main", "全体最適化の最大ループ回数, [1-50], デフォルト: %1")
            .arg(defaults.trwsPami.globalMaxLoops),
        "int");
    al4Option.setDefaultValue(QString::number(defaults.trwsPami.globalMaxLoops));
    parser.addOption(al4Option);

    QCommandLineOption calcOption(
        QStringList() << "calc",
        QCoreApplication::translate("main", "どこまで計算を行うか, [0-2], デフォルト: 0, 0: しない, 1: 位相相関法まで, 2: 最適化まで"),
        "int");
    calcOption.setDefaultValue("0");
    parser.addOption(calcOption);

    QCommandLineOption miOption(
        "make_image", QCoreApplication::translate("main", "画像を作成"));
    parser.addOption(miOption);

    QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        QCoreApplication::translate("main", "結合画像の出力先パス"),
        "file");
    parser.addOption(outputOption);

    QCommandLineOption exiOption(
        "export", QCoreApplication::translate("main", "画像をエクスポート"));
    parser.addOption(exiOption);

    QCommandLineOption closeOption(
        "close",
        QCoreApplication::translate("main", "計算結果が良好な場合、画像出力後にウインドウを閉じる"));
    parser.addOption(closeOption);

    parser.process(a);

    QSplashScreen splash(QPixmap(":/splash.png"));
    splash.show();
    a.processEvents();

    MainWindow w;
    w.resize(AppSettings::windowSize(QStringLiteral("mainWindow"),
                                     QSize(1200, 750)));

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
    /*
    if (parser.isSet(autoOption)) {
        manu = 0;
    }
    if (parser.isSet(manualOption)) {
        manu = 1;
    }
    */
    bool ok = false;
    int o_x = parser.value(overxOption).toInt(&ok);
    if (!ok || o_x < 1 || o_x > 100) {
        o_x = defaults.alignment.horizontalOverlapPercent;
    }
    ok = false;
    int o_y = parser.value(overyOption).toInt(&ok);
    if (!ok || o_y < 1 || o_y > 100) {
        o_y = defaults.alignment.verticalOverlapPercent;
    }
    ok = false;
    int o_r = parser.value(overrOption).toInt(&ok);
    if (!ok || o_r < 0 || o_r > 100) {
        o_r = defaults.alignment.searchRangePercent;
    }
    ok = false;
    int ar = parser.value(arrayOption).toInt(&ok);
    if (!ok || ar < 1 || ar > 8) {
        ar = defaults.arrangement.direction;
    }
    ok = false;
    int arh = parser.value(arrayhOption).toInt(&ok);
    if (!ok || arh < 0) {
        arh = defaults.arrangement.horizontalImageCount;
    }
    ok = false;
    int arv = parser.value(arrayvOption).toInt(&ok);
    if (!ok || arv < 0) {
        arv = defaults.arrangement.verticalImageCount;
    }
    ok = false;
    int zig = parser.value(zigOption).toInt(&ok);
    if (!ok || zig < 0 || zig > 1) {
        zig = defaults.arrangement.zigzag ? 1 : 0;
    }
    ok = false;
    int pa0 = parser.value(pa0Option).toInt(&ok);
    if (!ok || pa0 < 0 || pa0 > 1) {
        pa0 = defaults.trwsPami.localEnabled ? 1 : 0;
    }
    ok = false;
    int pa1 = parser.value(pa1Option).toInt(&ok);
    if (!ok || pa1 < 4 || pa1 > 100) {
        pa1 = defaults.trwsPami.localImageCount;
    }
    ok = false;
    int pa2 = parser.value(pa2Option).toInt(&ok);
    if (!ok || pa2 < 1 || pa2 > 10) {
        pa2 = defaults.trwsPami.localSearchRadius;
    }
    ok = false;
    int pa3 = parser.value(pa3Option).toInt(&ok);
    if (!ok || pa3 < 20 || pa3 > 100000) {
        pa3 = defaults.trwsPami.localMaxIterations;
    }
    ok = false;
    int pa4 = parser.value(pa4Option).toInt(&ok);
    if (!ok || pa4 < 1 || pa4 > 20) {
        pa4 = defaults.trwsPami.localMaxLoops;
    }
    ok = false;
    int al0 = parser.value(al0Option).toInt(&ok);
    if (!ok || al0 < 0 || al0 > 1) {
        al0 = defaults.trwsPami.globalEnabled ? 1 : 0;
    }
    ok = false;
    int al2 = parser.value(al2Option).toInt(&ok);
    if (!ok || al2 < 1 || al2 > 10) {
        al2 = defaults.trwsPami.globalSearchRadius;
    }
    ok = false;
    int al3 = parser.value(al3Option).toInt(&ok);
    if (!ok || al3 < 20 || al3 > 100000) {
        al3 = defaults.trwsPami.globalMaxIterations;
    }
    ok = false;
    int al4 = parser.value(al4Option).toInt(&ok);
    if (!ok || al4 < 1 || al4 > 50) {
        al4 = defaults.trwsPami.globalMaxLoops;
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
    const bool defaultLayoutLocked = defaults.canvas.layoutLocked;
    const bool defaultUseCanvasAsSource = defaults.canvas.useCanvasAsSource;

    QObject::connect(&w, &MainWindow::fileInputFinished, &w,
                     [&w,&splash,manu,o_x,o_y,o_r,ar,arh,arv,zig,pa0,pa1,pa2,pa3,pa4,al0,al2,al3,al4,ca,&out_p,closeTF,defaultLayoutLocked,defaultUseCanvasAsSource]() {
                        w.set_over_value(o_x, o_y, o_r);
                        w.set_array_value(ar, arh, arv);
                        w.set_zigzag_value(zig);
                        w.set_output_path(out_p);
                        w.set_close_value(closeTF);
                        w.set_opti_value(pa0,pa1,pa2,pa3,pa4,al0,al2,al3,al4);
                        w.set_canvas_state(defaultLayoutLocked, defaultUseCanvasAsSource);
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
