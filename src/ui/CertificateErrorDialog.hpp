#pragma once

#include <QDialog>
#include <QUrl>
#include <QWebEngineCertificateError>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

namespace virgin::ui {

class CertificateErrorDialog final : public QDialog {
    Q_OBJECT
public:
    explicit CertificateErrorDialog(const QWebEngineCertificateError& error, QWidget* parent = nullptr);

    bool shouldOverride() const { return override_; }

private:
    bool override_ = false;
};

} // namespace virgin::ui
