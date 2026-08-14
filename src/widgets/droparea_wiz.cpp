#include "droparea_wiz.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QMimeData>
#include <QUrl>
#include <QVBoxLayout>
#include <QLabel>

DropArea_wiz::DropArea_wiz(QWidget *parent) : QFrame(parent)
{
    setAcceptDrops(true);
    setMinimumHeight(120);

    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);

    updateDropAreaStyle(false);

    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel(tr("ここにファイルをドラッグ＆ドロップ"), this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
}

void DropArea_wiz::updateDropAreaStyle(bool dragActive)
{
    const QString childReset =
        "DropArea_wiz QLabel { border: none; background: transparent; }"
        "DropArea_wiz QWidget { border: none; background: transparent; }"
        "DropArea_wiz QFrame { border: none; background: transparent; }";

    if (dragActive) {
        setStyleSheet(
            "DropArea_wiz {"
            "  border: 2px dashed #2E86DE;"
            "  border-radius: 8px;"
            "  background-color: rgba(46, 134, 222, 40);"
            "}"
            + childReset
            );
    } else {
        setStyleSheet(
            "DropArea_wiz {"
            "  border: 2px dashed #888;"
            "  border-radius: 8px;"
            "  background-color: transparent;"
            "}"
            + childReset
            );
    }
}

void DropArea_wiz::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        updateDropAreaStyle(true);
    } else {
        event->ignore();
        updateDropAreaStyle(false);
    }
}

void DropArea_wiz::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropArea_wiz::dropEvent(QDropEvent *event)
{
    updateDropAreaStyle(false);
    QStringList input_files;
    const QList<QUrl> urls = event->mimeData()->urls();

    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            input_files << url.toLocalFile();
        }
    }

    if (!input_files.isEmpty()) {
        m_files = input_files;
        emit filesDropped(m_files);
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DropArea_wiz::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    updateDropAreaStyle(false);
}


