#ifndef DROPAREA_WIZ_H
#define DROPAREA_WIZ_H

#include <QFrame>
#include <QStringList>

class DropArea_wiz : public QFrame
{
    Q_OBJECT
public:
    explicit DropArea_wiz(QWidget *parent = nullptr);

    //QStringList input_files() const { return m_files; }

signals:
    void filesDropped(const QStringList &input_files);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;

private:
    void updateDropAreaStyle(bool dragActive);
    QStringList m_files;
};

#endif // DROPAREA_WIZ_H
