#pragma once

#include <QWidget>
#include <QUrl>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace virgin::ui {

class PrivacyPanel final : public QWidget {
    Q_OBJECT
public:
    explicit PrivacyPanel(QWidget* parent = nullptr);

    void setUrl(const QUrl& url);
    void setStats(int total, int blocked, int trackers, int ads);
    void setProtectionLevel(const QString& level);
    void setProtectionDetails(const QString& cookies,
                              const QString& canvas,
                              const QString& webrtc,
                              const QString& permissions);

signals:
    void siteControlsRequested(const QUrl& site);
    void clearDataRequested(const QUrl& site);
    void forgetSiteRequested(const QUrl& site);

private:
    QUrl currentUrl_;
    QLabel* domainLabel_ = nullptr;
    QLabel* protectionLabel_ = nullptr;
    QLabel* statsLabel_ = nullptr;
    QLabel* cookieLabel_ = nullptr;
    QLabel* canvasLabel_ = nullptr;
    QLabel* webrtcLabel_ = nullptr;
    QLabel* permissionsLabel_ = nullptr;
    QPushButton* siteControlsBtn_ = nullptr;
    QPushButton* clearDataBtn_ = nullptr;
    QPushButton* forgetBtn_ = nullptr;
};

} // namespace virgin::ui
