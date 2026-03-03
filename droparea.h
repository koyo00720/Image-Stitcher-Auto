#ifndef DROPAREA_H
#define DROPAREA_H

#include <QWidget>
#include <QStringList>

class DropArea : public QWidget {
    Q_OBJECT
public:
    explicit DropArea(QWidget* parent = nullptr);

    void setHintText(const QString& text) { hintText_ = text; update(); }

signals:
    void filesDropped(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    QString hintText_ = QStringLiteral("ここにドラッグ＆ドロップ");
    bool dragging_ = false;
};



#endif // DROPAREA_H
