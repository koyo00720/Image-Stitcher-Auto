#include "canvas_history_graph_widget.h"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QSizePolicy>
#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <utility>

CanvasHistoryGraphWidget::CanvasHistoryGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(520, 160);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setAutoFillBackground(true);
}

void CanvasHistoryGraphWidget::setProviders(
    std::function<QVector<CanvasHistoryNode>()> nodesFn,
    std::function<int()> currentFn,
    std::function<void(int)> activateFn)
{
    nodesProvider = std::move(nodesFn);
    currentProvider = std::move(currentFn);
    activateNode = std::move(activateFn);
    refresh();
}

void CanvasHistoryGraphWidget::refresh()
{
    nodes = nodesProvider ? nodesProvider() : QVector<CanvasHistoryNode>();
    currentNode = currentProvider ? currentProvider() : -1;
    layoutGraph();
    update();
}

QRect CanvasHistoryGraphWidget::currentNodeRect() const
{
    if (currentNode < 0 ||
        currentNode >= centers.size() ||
        !hasCenter.value(currentNode)) {
        return QRect();
    }

    const QPointF center = centers[currentNode];
    const QRectF rect(center.x() - nodeRadius,
                      center.y() - nodeRadius,
                      nodeRadius * 2.0,
                      nodeRadius * 2.0);
    return rect.toAlignedRect();
}

QColor CanvasHistoryGraphWidget::phaseMarkerColor()
{
    return QColor(64, 156, 255, 88);
}

QColor CanvasHistoryGraphWidget::optimizationMarkerColor()
{
    return QColor(255, 177, 66, 98);
}

QColor CanvasHistoryGraphWidget::currentMarkerColor()
{
    return QColor(36, 174, 78);
}

void CanvasHistoryGraphWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), palette().color(QPalette::Window));

    if (nodes.isEmpty()) {
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(rect(), Qt::AlignCenter,
                         QCoreApplication::translate("CanvasHistoryGraphWidget", "履歴なし"));
        return;
    }

    for (int i = 0; i < nodes.size(); ++i) {
        if (!hasCenter.value(i)) continue;
        const QPointF from = centers[i];
        for (int child : nodes[i].children) {
            if (child < 0 || child >= nodes.size() || !hasCenter.value(child)) continue;
            drawEdge(painter, from, centers[child], nodes[i].activeChild == child);
        }
    }

    for (int i = 0; i < nodes.size(); ++i) {
        if (!hasCenter.value(i)) continue;
        drawNode(painter, i);
    }
}

void CanvasHistoryGraphWidget::mousePressEvent(QMouseEvent* event)
{
    for (int i = 0; i < centers.size(); ++i) {
        if (!hasCenter.value(i)) continue;
        const QLineF distance(centers[i], event->position());
        if (distance.length() <= nodeRadius + 5.0) {
            if (activateNode) {
                activateNode(i);
            }
            refresh();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void CanvasHistoryGraphWidget::layoutGraph()
{
    centers.fill(QPointF(), nodes.size());
    hasCenter.fill(false, nodes.size());

    if (nodes.isEmpty()) {
        setMinimumSize(520, 160);
        return;
    }

    QVector<bool> visited(nodes.size(), false);
    QVector<bool> visiting(nodes.size(), false);
    int leafRow = 0;
    int maxDepth = 0;

    auto validIndex = [this](int index) {
        return index >= 0 && index < nodes.size();
    };

    std::function<qreal(int, int)> walk = [&](int index, int depth) -> qreal {
        if (!validIndex(index)) return topMargin + leafRow * rowSpacing;
        if (visited[index]) return centers[index].y();
        if (visiting[index]) {
            return topMargin + leafRow * rowSpacing;
        }

        visiting[index] = true;
        maxDepth = std::max(maxDepth, depth);

        QVector<int> validChildren;
        for (int child : nodes[index].children) {
            if (validIndex(child)) {
                validChildren.push_back(child);
            }
        }

        qreal y = 0.0;
        if (validChildren.isEmpty()) {
            y = topMargin + leafRow * rowSpacing;
            ++leafRow;
        } else {
            qreal firstY = 0.0;
            qreal lastY = 0.0;
            for (int childIndex = 0; childIndex < validChildren.size(); ++childIndex) {
                const qreal childY = walk(validChildren[childIndex], depth + 1);
                if (childIndex == 0) firstY = childY;
                lastY = childY;
            }
            y = (firstY + lastY) / 2.0;
        }

        centers[index] = QPointF(leftMargin + depth * columnSpacing, y);
        hasCenter[index] = true;
        visiting[index] = false;
        visited[index] = true;
        return y;
    };

    QVector<int> roots;
    for (int i = 0; i < nodes.size(); ++i) {
        if (!validIndex(nodes[i].parent)) {
            roots.push_back(i);
        }
    }
    if (roots.isEmpty()) {
        roots.push_back(0);
    }

    for (int root : std::as_const(roots)) {
        if (!visited[root]) {
            walk(root, 0);
            if (root != roots.last()) {
                ++leafRow;
            }
        }
    }
    for (int i = 0; i < nodes.size(); ++i) {
        if (!visited[i]) {
            walk(i, 0);
        }
    }

    const int rows = std::max(1, leafRow);
    const int graphWidth = static_cast<int>(leftMargin * 2 + (maxDepth + 1) * columnSpacing);
    const int graphHeight = static_cast<int>(topMargin * 2 + rows * rowSpacing);
    setMinimumSize(std::max(520, graphWidth), std::max(160, graphHeight));
}

void CanvasHistoryGraphWidget::drawEdge(QPainter& painter, const QPointF& from,
                                        const QPointF& to, bool active) const
{
    const QColor lineColor = active
                                 ? palette().color(QPalette::WindowText)
                                 : palette().color(QPalette::Mid);
    QPen pen(lineColor, active ? 2.0 : 1.25, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QPointF start(from.x() + nodeRadius + 2.0, from.y());
    const QPointF end(to.x() - nodeRadius - 2.0, to.y());
    const qreal middleX = (start.x() + end.x()) / 2.0;

    if (std::abs(start.y() - end.y()) < 1.0) {
        painter.drawLine(start, end);
    } else {
        painter.drawLine(start, QPointF(middleX, start.y()));
        painter.drawLine(QPointF(middleX, start.y()), QPointF(middleX, end.y()));
        painter.drawLine(QPointF(middleX, end.y()), end);
    }

    const qreal arrowSize = 5.0;
    painter.drawLine(end, QPointF(end.x() - arrowSize, end.y() - arrowSize));
    painter.drawLine(end, QPointF(end.x() - arrowSize, end.y() + arrowSize));
}

void CanvasHistoryGraphWidget::drawNode(QPainter& painter, int index) const
{
    const bool current = index == currentNode;
    const QColor highlightGreen = currentMarkerColor();
    const QColor normalText = palette().color(QPalette::WindowText);
    const QColor fillColor = palette().color(QPalette::Base);
    const QColor penColor = current ? highlightGreen : normalText;

    const QRectF circle(centers[index].x() - nodeRadius,
                        centers[index].y() - nodeRadius,
                        nodeRadius * 2.0,
                        nodeRadius * 2.0);

    painter.setBrush(fillColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(circle);

    const bool phase = (nodes[index].markers & CanvasHistoryMarkerPhase) != 0;
    const bool optimization = (nodes[index].markers & CanvasHistoryMarkerOptimization) != 0;
    if (phase || optimization) {
        QPainterPath clipPath;
        clipPath.addEllipse(circle.adjusted(2.0, 2.0, -2.0, -2.0));
        painter.save();
        painter.setClipPath(clipPath);
        painter.setPen(Qt::NoPen);
        painter.fillRect(circle, optimization ? optimizationMarkerColor() : phaseMarkerColor());
        painter.restore();
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(penColor, current ? 2.5 : 1.5));
    painter.drawEllipse(circle);

    QFont textFont = font();
    textFont.setBold(current);
    if (nodes[index].sequence >= 100) {
        textFont.setPointSizeF(std::max(7.0, textFont.pointSizeF() - 2.0));
    }
    painter.setFont(textFont);
    painter.setPen(current ? highlightGreen : normalText);
    painter.drawText(circle, Qt::AlignCenter, QString::number(nodes[index].sequence));
}
