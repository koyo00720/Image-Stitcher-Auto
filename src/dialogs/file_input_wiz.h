#ifndef FILE_INPUT_WIZ_H
#define FILE_INPUT_WIZ_H

#include <QDialog>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui { class FileInputDialog; }
QT_END_NAMESPACE


class FileInputDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FileInputDialog(const QStringList &initialFiles, QWidget *parent = nullptr);
    ~FileInputDialog();

    QStringList selectedFiles() const { return fiw_files; }

private slots:
    void onBrowseClicked();
    void onFilesDropped(const QStringList &files);

private:
    Ui::FileInputDialog *ui;

    // ファイルリストのUIを更新する
    void updateTable();

     // Tableの情報をm_filesへ同期する
    void syncMFilesFromTable();

    // 並べ替えボタン
    void onSortClicked();

    // 重複を削除ボタン
    void onDupDelClicked();

    void onFilesReceived(const QStringList &files);

    // ファイル名昇順ソート
    QStringList onSortUpFname(QStringList);

    QStringList fiw_files;
};
#endif
