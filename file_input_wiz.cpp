#include "file_input_wiz.h"
#include "ui_file_input_wiz.h"
#include "droparea_wiz.h"
#include "filetablewidget.h"

#include <QFileDialog>
#include <QAbstractItemModel>
#include <QListWidgetItem>
#include <QShortcut>

#include <QImageReader>
#include <QPixmap>
#include <QLabel>
#include <QFileInfo>
#include <QCollator>
#include <QDateTime>
#include <QDirIterator>
#include <QSet>

#include <algorithm>

FileInputDialog::FileInputDialog(const QStringList &initialFiles, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FileInputDialog)
    , fiw_files(initialFiles)
{
    ui->setupUi(this);

    ui->comboBox->addItem("ファイル名（昇順）");
    ui->comboBox->addItem("ファイル名（降順）");
    ui->comboBox->addItem("日付時刻（昇順）");
    ui->comboBox->addItem("日付時刻（降順）");

    connect(ui->tableWidgetFiles, &FileTableWidget::rowsReordered,
            this, [this]() {
                //renumberTableRows();
                syncMFilesFromTable();
            });

    ui->tableWidgetFiles->setColumnCount(2);
    ui->tableWidgetFiles->setHorizontalHeaderLabels(QStringList() << "プレビュー" << "パス");

    ui->tableWidgetFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetFiles->setSelectionMode(QAbstractItemView::ExtendedSelection); // または ExtendedSelection
    ui->tableWidgetFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidgetFiles->setSortingEnabled(false);

    ui->tableWidgetFiles->setColumnWidth(0, 50); // Preview
    ui->tableWidgetFiles->horizontalHeader()->setStretchLastSection(true); // Path
    ui->tableWidgetFiles->verticalHeader()->setDefaultSectionSize(35);

    ui->tableWidgetFiles->verticalHeader()->setFixedWidth(35);


    // 縦ヘッダを表示（番号として使う）
    ui->tableWidgetFiles->verticalHeader()->setVisible(true);

    connect(ui->tableWidgetFiles, &FileTableWidget::rowsReordered,
            this, [this]() {
                syncMFilesFromTable();
            });


    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, ui->tableWidgetFiles);
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        auto *sel = ui->tableWidgetFiles->selectionModel();
        if (!sel) return;

        QList<int> rows;
        for (const QModelIndex &idx : sel->selectedRows()) {
            rows.append(idx.row());
        }

        std::sort(rows.begin(), rows.end(), std::greater<int>());
        rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

        for (int row : rows) {
            ui->tableWidgetFiles->removeRow(row);
        }

        syncMFilesFromTable();
    });

    // シグナル接続
    connect(ui->browseButton, &QPushButton::clicked,
            this, &FileInputDialog::onBrowseClicked);

    connect(ui->dropArea, &DropArea_wiz::filesDropped,
            this, &FileInputDialog::onFilesDropped);

    connect(ui->sortButton, &QPushButton::clicked,
            this, &FileInputDialog::onSortClicked);

    connect(ui->dupButton, &QPushButton::clicked,
            this, &FileInputDialog::onDupDelClicked);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);

    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    updateTable();
}


FileInputDialog::~FileInputDialog()
{
    delete ui;
}

void FileInputDialog::onBrowseClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("画像ファイルを選択"),
        QString(),
        tr("画像ファイル (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;すべてのファイル (*)")
        );

    if (!files.isEmpty()) {
        onFilesReceived(files);
    }
}

void FileInputDialog::onFilesDropped(const QStringList &files)
{
    // 再帰的展開
    QStringList result;
    QSet<QString> seen;

    for (const QString& path : files) {
        QFileInfo info(path);

        if (!info.exists()) {
            // 存在しないパスは無視
            continue;
        }

        if (info.isFile()) {
            QString filePath = info.absoluteFilePath();
            if (!seen.contains(filePath)) {
                result << filePath;
                seen.insert(filePath);
            }
        }
        else if (info.isDir()) {
            QDirIterator it(
                info.absoluteFilePath(),
                QDir::Files,                       // ファイルのみ取得
                QDirIterator::Subdirectories       // 再帰
                );

            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo fInfo(filePath);
                QString absPath = fInfo.absoluteFilePath();

                if (!seen.contains(absPath)) {
                    result << absPath;
                    seen.insert(absPath);
                }
            }
        }
    }
    result = onSortUpFname(result);
    onFilesReceived(result);
}

void FileInputDialog::onFilesReceived(const QStringList &files)
{
    // 置き換えたいなら代入、追加したいなら append
    fiw_files.append(files);

    // UI更新
    updateTable();
}

void FileInputDialog::updateTable()
{
    ui->tableWidgetFiles->clearContents();
    ui->tableWidgetFiles->setRowCount(0);

    const QSize thumbSize(50, 35);

    QStringList validFiles;
    validFiles.reserve(fiw_files.size());

    for (const QString &path : std::as_const(fiw_files)) {
        // 先に画像読み込み
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();

        // 読み取り不可ならスキップ（fiw_filesにも入れない）
        if (img.isNull()) {
            continue;
        }

        // 読めたファイルだけ保持
        validFiles.append(path);

        // 行追加
        const int row = ui->tableWidgetFiles->rowCount();
        ui->tableWidgetFiles->insertRow(row);

        // --- 列0: Preview ---
        auto *previewItem = new QTableWidgetItem();
        previewItem->setTextAlignment(Qt::AlignCenter);

        QPixmap px = QPixmap::fromImage(
            img.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            );
        previewItem->setData(Qt::DecorationRole, px);
        ui->tableWidgetFiles->setItem(row, 0, previewItem);

        // --- 列1: Path ---
        auto *pathItem = new QTableWidgetItem(path);
        pathItem->setData(Qt::UserRole, path);
        ui->tableWidgetFiles->setItem(row, 1, pathItem);
    }

    // fiw_files を「読めたものだけ」に更新
    fiw_files = validFiles;
}

void FileInputDialog::syncMFilesFromTable()
{
    QStringList newFiles;

    for (int row = 0; row < ui->tableWidgetFiles->rowCount(); ++row) {
        QTableWidgetItem *pathItem = ui->tableWidgetFiles->item(row, 1);
        if (!pathItem) continue;

        QString path = pathItem->data(Qt::UserRole).toString();
        if (path.isEmpty()) {
            path = pathItem->text();
        }

        if (!path.isEmpty()) {
            newFiles.append(path);
        }
    }

    fiw_files = newFiles;
}

void FileInputDialog::onDupDelClicked()
{
    fiw_files.removeDuplicates();
    updateTable();
}

void FileInputDialog::onSortClicked()
{
    QString text = ui->comboBox->currentText();
    if (text == "ファイル名（昇順）") {
        fiw_files = onSortUpFname(fiw_files);
        updateTable();
    } else if (text == "ファイル名（降順）") {
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);

        std::sort(fiw_files.begin(), fiw_files.end(),
                  [&collator](const QString &a, const QString &b) {
                      const QFileInfo ia(a);
                      const QFileInfo ib(b);

                      // 第1キー: 親パス（降順）
                      int cmp = collator.compare(ia.path(), ib.path());
                      if (cmp != 0) return cmp > 0;

                      // 第2キー: ファイル名（拡張子なし）（降順）
                      cmp = collator.compare(ia.completeBaseName(), ib.completeBaseName());
                      if (cmp != 0) return cmp > 0;

                      // 第3キー: 拡張子（降順）
                      cmp = collator.compare(ia.suffix(), ib.suffix());
                      if (cmp != 0) return cmp > 0;

                      // 最終タイブレーク: フルパス（降順）
                      return collator.compare(a, b) > 0;
                  });

        updateTable();
    } else if (text == "日付時刻（昇順）") {
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);

        std::sort(fiw_files.begin(), fiw_files.end(),
                  [&collator](const QString &a, const QString &b) {
                      const QFileInfo ia(a);
                      const QFileInfo ib(b);

                      const QDateTime ta = ia.lastModified();
                      const QDateTime tb = ib.lastModified();

                      if (ta != tb) {
                          return ta < tb;  // 古い -> 新しい
                      }

                      // 時刻が同じ場合のタイブレーク（見た目を安定化）
                      int cmp = collator.compare(ia.path(), ib.path());
                      if (cmp != 0) return cmp < 0;

                      cmp = collator.compare(ia.completeBaseName(), ib.completeBaseName());
                      if (cmp != 0) return cmp < 0;

                      cmp = collator.compare(ia.suffix(), ib.suffix());
                      if (cmp != 0) return cmp < 0;

                      return collator.compare(a, b) < 0;
                  });

        updateTable();
    } else if (text == "日付時刻（降順）") {
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);

        std::sort(fiw_files.begin(), fiw_files.end(),
                  [&collator](const QString &a, const QString &b) {
                      const QFileInfo ia(a);
                      const QFileInfo ib(b);

                      const QDateTime ta = ia.lastModified();
                      const QDateTime tb = ib.lastModified();

                      if (ta != tb) {
                          return ta > tb;  // 新しい -> 古い
                      }

                      // 同時刻のタイブレーク
                      int cmp = collator.compare(ia.path(), ib.path());
                      if (cmp != 0) return cmp < 0;

                      cmp = collator.compare(ia.completeBaseName(), ib.completeBaseName());
                      if (cmp != 0) return cmp < 0;

                      cmp = collator.compare(ia.suffix(), ib.suffix());
                      if (cmp != 0) return cmp < 0;

                      return collator.compare(a, b) < 0;
                  });
        updateTable();
    }
}


QStringList FileInputDialog::onSortUpFname(QStringList input_files)
{
    QCollator collator;
    collator.setNumericMode(true);          // "2" < "10" を自然に
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    std::sort(input_files.begin(), input_files.end(),
              [&collator](const QString &a, const QString &b) {
                  const QFileInfo ia(a);
                  const QFileInfo ib(b);

                  // 第1キー: 親パス
                  const QString dirA = ia.path();       // 例: C:/xxx/yyy
                  const QString dirB = ib.path();
                  int cmp = collator.compare(dirA, dirB);
                  if (cmp != 0) return cmp < 0;

                  // 第2キー: ファイル名（拡張子なし）
                  // completeBaseName() は "a.tar.gz" -> "a.tar"
                  const QString baseA = ia.completeBaseName();
                  const QString baseB = ib.completeBaseName();
                  cmp = collator.compare(baseA, baseB);
                  if (cmp != 0) return cmp < 0;

                  // 第3キー: 拡張子
                  const QString extA = ia.suffix();
                  const QString extB = ib.suffix();
                  cmp = collator.compare(extA, extB);
                  if (cmp != 0) return cmp < 0;

                  // 同値時の最終タイブレーク（安定化のためフルパス）
                  return collator.compare(a, b) < 0;
              });
    return input_files;
};
