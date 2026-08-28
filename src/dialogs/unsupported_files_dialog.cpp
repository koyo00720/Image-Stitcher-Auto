#include "unsupported_files_dialog.h"

#include "app_settings.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

void showUnsupportedFilesDialog(const QStringList& paths, QWidget* parent)
{
    if (paths.isEmpty()) {
        return;
    }

    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowTitle(QCoreApplication::translate(
        "UnsupportedFilesDialog", "読み取れなかったファイル"));
    dialog->resize(AppSettings::windowSize(
        QStringLiteral("unsupportedFiles"), QSize(760, 420)));

    auto* layout = new QVBoxLayout(dialog);
    auto* message = new QLabel(QCoreApplication::translate(
        "UnsupportedFilesDialog",
        "次のファイルは非対応、または読み取れなかったため入力されませんでした。"),
        dialog);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto* table = new QTableWidget(paths.size(), 3, dialog);
    table->setHorizontalHeaderLabels(
        {QCoreApplication::translate("UnsupportedFilesDialog", "フォルダ名"),
         QCoreApplication::translate("UnsupportedFilesDialog", "ファイル名（拡張子なし）"),
         QCoreApplication::translate("UnsupportedFilesDialog", "ファイル名")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setSortingEnabled(false);
    for (int row = 0; row < paths.size(); ++row) {
        const QFileInfo info(paths[row]);
        table->setItem(row, 0, new QTableWidgetItem(info.absolutePath()));
        table->setItem(row, 1, new QTableWidgetItem(info.completeBaseName()));
        table->setItem(row, 2, new QTableWidgetItem(info.fileName()));
    }
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->setSortingEnabled(true);
    layout->addWidget(table, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    if (QPushButton* closeButton = buttons->button(QDialogButtonBox::Close)) {
        closeButton->setText(QCoreApplication::translate(
            "UnsupportedFilesDialog", "閉じる"));
    }
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     dialog, &QDialog::close);
    layout->addWidget(buttons);

    QObject::connect(dialog, &QDialog::finished, dialog, [dialog]() {
        AppSettings::setWindowSize(QStringLiteral("unsupportedFiles"),
                                   dialog->size());
    });
    dialog->show();
    dialog->raise();
}
