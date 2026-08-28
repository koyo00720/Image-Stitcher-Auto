#ifndef OPTI_SETTINGS_H
#define OPTI_SETTINGS_H

#include <QDialog>
#include <vector>

namespace Ui {
class opti_settings;
}

class opti_settings : public QDialog
{
    Q_OBJECT

public:
    explicit opti_settings(QWidget *parent = nullptr);
    ~opti_settings();
    void setValues(int,int,int,int,int,int,int,bool,bool,bool,int,int);
    std::vector<int> getValues();
    std::vector<bool> getTFs();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void lock1(bool);
    void lock2(bool);
    void lock3(bool);

private:
    Ui::opti_settings *ui;
};

#endif // OPTI_SETTINGS_H
