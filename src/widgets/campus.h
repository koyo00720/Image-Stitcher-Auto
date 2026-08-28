#ifndef CAMPUS_H
#define CAMPUS_H

#include <QGraphicsView>
#include <QStringList>
#include <QWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class maincampus : public QGraphicsView
{
    Q_OBJECT
public:
    explicit maincampus(QWidget *parent = nullptr);
    void setCanvasBackground(bool white); // true=白, false=黒
    void set_Camera(const QRectF& target) noexcept;

signals:
    void zoomChanged(int percent);
    void filesDropped(const QStringList& paths);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    //void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRectF m_camera_target;
};

#endif // CAMPUS_H
