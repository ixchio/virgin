#pragma once

#include <QTabWidget>
#include <QUrl>
#include <QStack>
#include <vector>

namespace virgin::profiles { class VirginProfile; }
namespace virgin::storage { class HistoryStore; }

namespace virgin::browser {

class BrowserTab;

struct ClosedTab {
    QUrl url;
    QString title;
    int fromIndex = -1;
};

class TabManager final : public QTabWidget {
    Q_OBJECT
public:
    explicit TabManager(virgin::profiles::VirginProfile* profile,
                        virgin::storage::HistoryStore* history,
                        QWidget* parent = nullptr);
    ~TabManager() override;

    BrowserTab* currentTab() const;
    BrowserTab* tabAt(int index) const;
    int count() const { return QTabWidget::count(); }

    BrowserTab* createTab(const QUrl& url = QUrl(), bool makeActive = true);
    void closeTab(int index);
    void closeCurrentTab();
    void duplicateTab(int index);

    bool canReopen() const { return !closedStack_.isEmpty(); }
    void reopenLastClosedTab();
    int closedCount() const { return static_cast<int>(closedStack_.size()); }

    QList<QUrl> allTabUrls() const;
    QList<BrowserTab*> allTabs() const;
    int currentIndexSafe() const { return currentIndex(); }
    void restoreTabs(const QList<QUrl>& urls, int activeIndex = 0);
    void closeAllTabs();

signals:
    void activeTabChanged(BrowserTab* tab);
    void tabCountChanged(int count);
    void requestNewWindow(const QUrl& url, bool isPrivate);

private slots:
    void onCurrentChanged(int index);
    void onTabCloseRequested(int index);
    void onTabTitleChanged(const QString& title);
    void onTabUrlChanged(const QUrl& url);

private:
    virgin::profiles::VirginProfile* profile_ = nullptr;
    virgin::storage::HistoryStore* history_ = nullptr;
    QStack<ClosedTab> closedStack_;
    static constexpr int kMaxClosed = 25;
};

} // namespace virgin::browser
