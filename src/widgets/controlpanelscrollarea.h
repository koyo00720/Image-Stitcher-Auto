#ifndef CONTROLPANELSCROLLAREA_H
#define CONTROLPANELSCROLLAREA_H

#include <QApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>

class ControlPanelScrollArea final : public QScrollArea
{
public:
    explicit ControlPanelScrollArea(QWidget* parent = nullptr)
        : QScrollArea(parent)
    {
    }

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        QScrollBar* scrollBar = verticalScrollBar();
        if (scrollBar && scrollBar->maximum() > scrollBar->minimum()) {
            int delta = event->pixelDelta().y();
            if (delta == 0) {
                const int singleStep = std::max(1, scrollBar->singleStep());
                delta = event->angleDelta().y()
                        * QApplication::wheelScrollLines() * singleStep / 120;
            }
            if (event->inverted()) {
                delta = -delta;
            }
            if (delta != 0) {
                scrollBar->setValue(scrollBar->value() - delta);
                event->accept();
                return;
            }
        }
        QScrollArea::wheelEvent(event);
    }
};

#endif // CONTROLPANELSCROLLAREA_H
