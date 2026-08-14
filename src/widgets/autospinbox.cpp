#include "autospinbox.h"

AutoSpinBox::AutoSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
}

QString AutoSpinBox::textFromValue(int value) const
{
    if (value == 0) {
        return "Auto";
    }
    return QSpinBox::textFromValue(value);
}

int AutoSpinBox::valueFromText(const QString &text) const
{
    if (text.compare("Auto", Qt::CaseInsensitive) == 0) {
        return 0;
    }
    return QSpinBox::valueFromText(text);
}

QValidator::State AutoSpinBox::validate(QString &text, int &pos) const
{
    Q_UNUSED(pos);

    if (text.isEmpty()) {
        return QValidator::Intermediate;
    }

    if (text.compare("Auto", Qt::CaseInsensitive) == 0) {
        return QValidator::Acceptable;
    }

    if (QStringLiteral("Auto").startsWith(text, Qt::CaseInsensitive)) {
        return QValidator::Intermediate;
    }

    return QSpinBox::validate(text, pos);
}
