#include "DropArea.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QPainter>

DropArea::DropArea(QWidget* parent) : QWidget(parent)
{
    setAcceptDrops(true);
    setMinimumSize(200, 120);
}

void DropArea::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) {
        dragging_ = true;
        update();
        e->acceptProposedAction();
    }
}

void DropArea::dragMoveEvent(QDragMoveEvent* e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void DropArea::dragLeaveEvent(QDragLeaveEvent*)
{
    dragging_ = false;
    update();
}

void DropArea::dropEvent(QDropEvent* e)
{
    dragging_ = false;
    update();

    if (!e->mimeData()->hasUrls()) return;

    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) continue;
        const QString p = u.toLocalFile();
        paths << p;
    }

    if (!paths.isEmpty()) emit filesDropped(paths);
    e->acceptProposedAction();
}

void DropArea::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 背景（ドラッグ中だけ少し強調）
    if (dragging_) {
        p.fillRect(rect(), palette().highlight().color().lighter(180));
    } else {
        p.fillRect(rect(), palette().window());
    }

    // 枠線（点線）
    QPen pen(dragging_ ? palette().highlight().color() : palette().mid().color());
    pen.setWidth(2);
    pen.setStyle(Qt::DashLine);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    QRect r = rect().adjusted(6, 6, -6, -6);
    p.drawRoundedRect(r, 10, 10);

    // 文字
    p.setPen(palette().text().color());
    QFont f = font();
    f.setPointSize(std::max(10, f.pointSize()));  // 小さすぎ防止
    p.setFont(f);

    const QString text = hintText_;
    p.drawText(r.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap, text);
}
