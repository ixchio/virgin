#include "CertificateErrorDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>

namespace virgin::ui {

CertificateErrorDialog::CertificateErrorDialog(const QWebEngineCertificateError& error, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Your connection is not secure");
    setModal(true);
    resize(520, 340);

    auto* layout = new QVBoxLayout(this);

    auto* icon = new QLabel("Virgin stopped this connection", this);
    icon->setObjectName("warningBlock");
    layout->addWidget(icon);

    auto* urlLabel = new QLabel(QString("URL: %1").arg(error.url().toString()), this);
    urlLabel->setObjectName("mutedText");
    urlLabel->setWordWrap(true);
    layout->addWidget(urlLabel);

    auto* errLabel = new QLabel(QString("Error: %1").arg(error.description()), this);
    errLabel->setWordWrap(true);
    errLabel->setObjectName("statBlock");
    layout->addWidget(errLabel);

    auto* detail = new QLabel(
        "The site's certificate could not prove its identity. Attackers may be trying to read or change information sent to this site.<br><br>"
        "Go back unless you understand why this certificate is invalid.",
        this);
    detail->setTextFormat(Qt::RichText);
    detail->setWordWrap(true);
    layout->addWidget(detail);

    auto* overridableLabel = new QLabel(
        error.isOverridable() ? "Advanced users may continue once, but the connection may be unsafe."
                             : "This error cannot be bypassed.",
        this);
    overridableLabel->setObjectName(error.isOverridable() ? "protectionGood" : "warningBlock");
    overridableLabel->setWordWrap(true);
    layout->addWidget(overridableLabel);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* backBtn = new QPushButton("Go back");
    btnRow->addWidget(backBtn);

    QPushButton* overrideBtn = nullptr;
    if (error.isOverridable()) {
        overrideBtn = new QPushButton("Continue once (unsafe)", this);
        overrideBtn->setProperty("danger", true);
        btnRow->addWidget(overrideBtn);
        connect(overrideBtn, &QPushButton::clicked, [this]{ override_ = true; accept(); });
    }

    layout->addLayout(btnRow);
    connect(backBtn, &QPushButton::clicked, [this]{ override_ = false; reject(); });

}

} // namespace virgin::ui
