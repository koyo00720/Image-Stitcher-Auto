#ifndef CANVAS_HISTORY_GRAPH_WIDGET_H
#define CANVAS_HISTORY_GRAPH_WIDGET_H

#include <QPoint>
#include <QPointF>
#include <QColor>
#include <QRect>
#include <QVector>
#include <QWidget>

#include <functional>

class QMouseEvent;
class QPaintEvent;
class QPainter;

enum CanvasHistoryMarker {
    CanvasHistoryMarkerNone = 0,
    CanvasHistoryMarkerPhase = 1 << 0,
    CanvasHistoryMarkerOptimization = 1 << 1
};

struct CanvasHistoryNode {
    QVector<QPoint> positions;
    int parent = -1;
    QVector<int> children;
    int activeChild = -1;
    int sequence = 0;
    int markers = CanvasHistoryMarkerNone;
};

class CanvasHistoryGraphWidget : public QWidget
{
public:
    explicit CanvasHistoryGraphWidget(QWidget* parent = nullptr);

    void setProviders(std::function<QVector<CanvasHistoryNode>()> nodesFn,
                      std::function<int()> currentFn,
                      std::function<void(int)> activateFn);
    void refresh();
    QRect currentNodeRect() const;
    static QColor phaseMarkerColor();
    static QColor optimizationMarkerColor();
    static QColor currentMarkerColor();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void layoutGraph();
    void drawEdge(QPainter& painter, const QPointF& from, const QPointF& to,
                  bool active) const;
    void drawNode(QPainter& painter, int index) const;

    QVector<CanvasHistoryNode> nodes;
    int currentNode = -1;
    QVector<QPointF> centers;
    QVector<bool> hasCenter;
    std::function<QVector<CanvasHistoryNode>()> nodesProvider;
    std::function<int()> currentProvider;
    std::function<void(int)> activateNode;

    static constexpr qreal nodeRadius = 17.0;
    static constexpr qreal leftMargin = 42.0;
    static constexpr qreal topMargin = 42.0;
    static constexpr qreal columnSpacing = 70.0;
    static constexpr qreal rowSpacing = 64.0;
};

#endif // CANVAS_HISTORY_GRAPH_WIDGET_H
