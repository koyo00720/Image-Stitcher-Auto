#include "campus.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QBrush>
#include <QColor>
#include <cmath>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

maincampus::maincampus(QWidget *parent) : QGraphicsView(parent)
{
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    //setFocusPolicy(Qt::StrongFocus);
}

void maincampus::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void maincampus::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void maincampus::dropEvent(QDropEvent* event)
{
    QStringList localPaths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            localPaths.push_back(url.toLocalFile());
        }
    }
    if (!localPaths.isEmpty()) {
        emit filesDropped(localPaths);
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dropEvent(event);
}


static int zoomPercent(const QGraphicsView *v)
{
    return int(std::round(v->transform().m11() * 100.0));
}


void maincampus::wheelEvent(QWheelEvent *event)
{
    const auto mods = event->modifiers();

    // Ctrl + ホイール：ズーム
    if (mods & Qt::ControlModifier) {
        const double zoomFactor = 1.05;
        if (event->angleDelta().y() > 0)
            scale(zoomFactor, zoomFactor);
        else
            scale(1.0 / zoomFactor, 1.0 / zoomFactor);

        event->accept();

        emit zoomChanged(zoomPercent(this)); // 拡大率表示
        return;
    }

    // Shift + ホイール：横スクロール（現状維持）
    if (mods & Qt::ShiftModifier) {
        int delta = event->angleDelta().x();
        if (delta == 0) delta = event->angleDelta().y();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta);
        event->accept();
        return;
    }

    // 何も押してない：通常の縦スクロール
    QGraphicsView::wheelEvent(event);
}

// 背景色を設定
void maincampus::setCanvasBackground(bool white)
{
    setBackgroundBrush(white ? QBrush(Qt::white) : QBrush(Qt::black));
}

void maincampus::set_Camera(const QRectF& target) noexcept
{
    m_camera_target = target;
}

// ウインドウがリサイズしてもターゲットが常に見えるようにする
void maincampus::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_camera_target.isNull()) {
        // 中心合わせ＋リサイズ
        //fitInView(m_camera_target, Qt::KeepAspectRatio);
        // 中心合わせのみ
        centerOn(m_camera_target.center());
    }
}
