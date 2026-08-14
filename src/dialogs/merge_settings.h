#ifndef MERGE_SETTINGS_H
#define MERGE_SETTINGS_H

#include <QDialog>

namespace Ui {
class merge_settings;
}

class merge_settings : public QDialog
{
    Q_OBJECT

public:
    explicit merge_settings(QWidget *parent = nullptr);
    ~merge_settings();

    void setMode(int mode);
    int getMode() const;

private:
    Ui::merge_settings *ui;
};

#endif // MERGE_SETTINGS_H
