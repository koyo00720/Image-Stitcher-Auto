#ifndef LEAST_SQUARES_SETTINGS_H
#define LEAST_SQUARES_SETTINGS_H

#include <QDialog>
#include <array>

namespace Ui {
class least_squares_settings;
}

class least_squares_settings : public QDialog
{
    Q_OBJECT

public:
    explicit least_squares_settings(QWidget *parent = nullptr);
    ~least_squares_settings();

    void setValues(double regressionThreshold,
                   double relativeThreshold,
                   double absoluteThreshold,
                   double maxPairErrorForRelative);
    std::array<double, 4> getValues() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    Ui::least_squares_settings *ui;
};

#endif // LEAST_SQUARES_SETTINGS_H
