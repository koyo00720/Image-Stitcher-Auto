#ifndef UNSUPPORTED_FILES_DIALOG_H
#define UNSUPPORTED_FILES_DIALOG_H

#include <QStringList>

class QWidget;

void showUnsupportedFilesDialog(const QStringList& paths,
                                QWidget* parent = nullptr);

#endif // UNSUPPORTED_FILES_DIALOG_H
