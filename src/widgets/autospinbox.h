#ifndef AUTOSPINBOX_H
#define AUTOSPINBOX_H

#include <QSpinBox>
#include <QValidator>

class AutoSpinBox : public QSpinBox
{
    Q_OBJECT

public:
    explicit AutoSpinBox(QWidget *parent = nullptr);

protected:
    QString textFromValue(int value) const override;
    int valueFromText(const QString &text) const override;
    QValidator::State validate(QString &text, int &pos) const override;
};

#endif // AUTOSPINBOX_H
