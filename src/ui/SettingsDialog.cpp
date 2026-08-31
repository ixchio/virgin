#include "SettingsDialog.hpp"
#include "storage/SettingsStore.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>

namespace virgin::ui {

SettingsDialog::SettingsDialog(virgin::storage::SettingsStore* settings, QWidget* parent)
    : QDialog(parent), settings_(settings)
{
    setWindowTitle("Virgin — Settings");
    resize(560, 560);

    auto* layout = new QVBoxLayout(this);

    auto* privacyGroup = new QGroupBox("Privacy", this);
    auto* pv = new QVBoxLayout(privacyGroup);
    strictBox_ = new QCheckBox("Strict mode (block canvas reads and third-party cookies)", this);
    strictBox_->setChecked(settings_->strictMode());
    strictBox_->setToolTip("May break sites that depend on cross-site cookies or canvas access.");
    pv->addWidget(strictBox_);

    auto* cookieRow = new QHBoxLayout();
    cookieRow->addWidget(new QLabel("Third-party cookies:", this));
    cookieCombo_ = new QComboBox(this);
    cookieCombo_->addItems({"Standard (allow browser cookies)", "Strict (block third-party cookies)", "Session-only (delete cookies on exit)"});
    QString cm = settings_->value("cookie_mode", "standard").toString();
    if (cm == "strict") cookieCombo_->setCurrentIndex(1);
    else if (cm == "ephemeral") cookieCombo_->setCurrentIndex(2);
    else cookieCombo_->setCurrentIndex(0);
    cookieRow->addWidget(cookieCombo_);
    cookieRow->addStretch();
    pv->addLayout(cookieRow);

    webrtcBox_ = new QCheckBox("WebRTC: public interfaces only (prevent local IP leak)", this);
    webrtcBox_->setChecked(settings_->value("webrtc_public_only", true).toBool());
    webrtcBox_->setToolTip("Reduces exposure of local network addresses during calls. Private windows always use this setting.");
    pv->addWidget(webrtcBox_);

    httpsFirstBox_ = new QCheckBox("HTTPS upgrade (replace http:// with https:// before loading)", this);
    httpsFirstBox_->setChecked(settings_->value("https_first", true).toBool());
    pv->addWidget(httpsFirstBox_);

    adblockBox_ = new QCheckBox("Enable ad/tracker blocking (EasyList/EasyPrivacy)", this);
    adblockBox_->setChecked(settings_->value("adblock_enabled", true).toBool());
    adblockBox_->setToolTip("Includes extra first-party cleanup for YouTube video and display ads.");
    pv->addWidget(adblockBox_);

    auto* note = new QLabel("Private windows keep browsing state in memory and do not save Virgin history or sessions. Ctrl+Shift+X closes all private windows.", this);
    note->setWordWrap(true);
    note->setObjectName("mutedText");
    pv->addWidget(note);

    layout->addWidget(privacyGroup);

    auto* histGroup = new QGroupBox("History", this);
    auto* hv = new QHBoxLayout(histGroup);
    hv->addWidget(new QLabel("Keep history:", this));
    historyCombo_ = new QComboBox(this);
    historyCombo_->addItems({"forever","30 days","7 days","never"});
    int days = settings_->historyRetentionDays();
    if (days == -1) historyCombo_->setCurrentIndex(0);
    else if (days == 30) historyCombo_->setCurrentIndex(1);
    else if (days == 7) historyCombo_->setCurrentIndex(2);
    else if (days == 0) historyCombo_->setCurrentIndex(3);
    else historyCombo_->setCurrentIndex(0);
    hv->addWidget(historyCombo_);
    hv->addStretch();
    layout->addWidget(histGroup);

    auto* searchGroup = new QGroupBox("Search", this);
    auto* sv = new QVBoxLayout(searchGroup);
    auto* searchRow = new QHBoxLayout();
    searchRow->addWidget(new QLabel("Search engine:", this));
    searchCombo_ = new QComboBox(this);
    searchCombo_->addItem("Google", "https://www.google.com/search?q=%s");
    searchCombo_->addItem("DuckDuckGo (privacy default)", "https://duckduckgo.com/?q=%s");
    searchCombo_->addItem("Brave Search", "https://search.brave.com/search?q=%s");
    searchCombo_->addItem("Startpage", "https://www.startpage.com/sp/search?query=%s");
    searchCombo_->addItem("Bing", "https://www.bing.com/search?q=%s");
    searchCombo_->addItem("Custom HTTPS URL…", QString());
    searchRow->addWidget(searchCombo_, 1);
    sv->addLayout(searchRow);

    const QString currentSearch = settings_->searchEngineUrl();
    int searchIndex = searchCombo_->findData(currentSearch);
    if (searchIndex < 0) searchIndex = searchCombo_->count() - 1;
    searchCombo_->setCurrentIndex(searchIndex);

    searchCustomLabel_ = new QLabel("Custom URL (%s is replaced by your search):", this);
    searchEdit_ = new QLineEdit(currentSearch, this);
    searchEdit_->setPlaceholderText("https://example.com/search?q=%s");
    const bool customSearch = searchCombo_->currentData().toString().isEmpty();
    searchCustomLabel_->setVisible(customSearch);
    searchEdit_->setVisible(customSearch);
    sv->addWidget(searchCustomLabel_);
    sv->addWidget(searchEdit_);
    layout->addWidget(searchGroup);

    connect(searchCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        const bool custom = searchCombo_->currentData().toString().isEmpty();
        searchCustomLabel_->setVisible(custom);
        searchEdit_->setVisible(custom);
    });

    auto* internal = new QLabel("Browser pages: <a href='virgin://newtab'>New Tab</a> · "
                                "<a href='virgin://history'>virgin://history</a> · "
                                "<a href='virgin://version'>virgin://version</a> · "
                                "<a href='virgin://settings'>virgin://settings</a>", this);
    internal->setOpenExternalLinks(false);
    internal->setObjectName("mutedText");
    layout->addWidget(internal);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* cancel = new QPushButton("Cancel", this);
    auto* save = new QPushButton("Save", this);
    save->setDefault(true);
    save->setProperty("accent", true);
    btnRow->addWidget(cancel);
    btnRow->addWidget(save);
    layout->addLayout(btnRow);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, [this]{
        QString searchUrl = searchCombo_->currentData().toString();
        if (searchUrl.isEmpty()) searchUrl = searchEdit_->text();
        if (!virgin::storage::SettingsStore::isValidSearchEngineUrl(searchUrl)) {
            QMessageBox::warning(this, "Invalid search engine",
                "Use an HTTPS search URL containing %s where the search words belong.");
            return;
        }

        settings_->setStrictMode(strictBox_->isChecked());
        settings_->setValue("https_first", httpsFirstBox_->isChecked());
        settings_->setValue("adblock_enabled", adblockBox_->isChecked());
        settings_->setValue("webrtc_public_only", webrtcBox_->isChecked());
        settings_->setSearchEngineUrl(searchUrl);
        int idx = historyCombo_->currentIndex();
        int retentionDays = -1;
        if (idx==1) retentionDays=30; else if(idx==2) retentionDays=7; else if(idx==3) retentionDays=0;
        settings_->setValue("history_retention", retentionDays);
        QString cookieMode = "standard";
        if (cookieCombo_->currentIndex()==1) cookieMode="strict";
        else if (cookieCombo_->currentIndex()==2) cookieMode="ephemeral";
        settings_->setValue("cookie_mode", cookieMode);
        settings_->save();
        accept();
    });
}

} // namespace virgin::ui
