#include "PrivacyPanel.hpp"
#include <QFrame>

namespace virgin::ui {

PrivacyPanel::PrivacyPanel(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup);
    setFixedSize(340, 430);
    setObjectName("privacyPanel");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16,16,16,16);
    layout->setSpacing(8);

    domainLabel_ = new QLabel("example.com", this);
    domainLabel_->setObjectName("panelTitle");
    layout->addWidget(domainLabel_);

    protectionLabel_ = new QLabel("Protection: Standard", this);
    protectionLabel_->setObjectName("protectionGood");
    layout->addWidget(protectionLabel_);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    layout->addWidget(sep);

    statsLabel_ = new QLabel("This site\nRequests   0\nBlocked    0\nTrackers   0\nAds        0", this);
    statsLabel_->setObjectName("statBlock");
    layout->addWidget(statsLabel_);

    cookieLabel_ = new QLabel("Third-party cookies  —", this);
    canvasLabel_ = new QLabel("Canvas reads         —", this);
    webrtcLabel_ = new QLabel("WebRTC interfaces    —", this);
    for (auto* l : {cookieLabel_, canvasLabel_, webrtcLabel_}) layout->addWidget(l);

    permissionsLabel_ = new QLabel("Location    ASK\nCamera      ASK\nMicrophone  ASK", this);
    permissionsLabel_->setObjectName("mutedText");
    layout->addWidget(permissionsLabel_);

    siteControlsBtn_ = new QPushButton("Site settings…", this);
    clearDataBtn_ = new QPushButton("Clear site data…", this);
    forgetBtn_ = new QPushButton("Forget this site…", this);
    forgetBtn_->setProperty("danger", true);
    for (auto* b : {siteControlsBtn_, clearDataBtn_, forgetBtn_}) layout->addWidget(b);

    connect(siteControlsBtn_, &QPushButton::clicked, this,
            [this] { emit siteControlsRequested(currentUrl_); });
    connect(clearDataBtn_, &QPushButton::clicked, this,
            [this] { emit clearDataRequested(currentUrl_); });
    connect(forgetBtn_, &QPushButton::clicked, this,
            [this] { emit forgetSiteRequested(currentUrl_); });

    layout->addStretch();
}

void PrivacyPanel::setUrl(const QUrl& url) {
    currentUrl_ = url;
    domainLabel_->setText(url.host().isEmpty() ? url.toString() : url.host());
}

void PrivacyPanel::setStats(int total, int blocked, int trackers, int ads) {
    statsLabel_->setText(QString("This site\nRequests  %1\nBlocked   %2\nTrackers  %3\nAds       %4").arg(total).arg(blocked).arg(trackers).arg(ads));
}

void PrivacyPanel::setProtectionLevel(const QString& level) {
    QString display = level.toLower();
    if (!display.isEmpty()) display[0] = display[0].toUpper();
    protectionLabel_->setText("Protection: " + display);
}

void PrivacyPanel::setProtectionDetails(const QString& cookies,
                                        const QString& canvas,
                                        const QString& webrtc,
                                        const QString& permissions) {
    cookieLabel_->setText("Third-party cookies  " + cookies);
    canvasLabel_->setText("Canvas reads         " + canvas);
    webrtcLabel_->setText("WebRTC interfaces    " + webrtc);
    permissionsLabel_->setText(permissions);
}

} // namespace virgin::ui
