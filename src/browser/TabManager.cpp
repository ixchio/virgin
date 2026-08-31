#include "TabManager.hpp"
#include "BrowserTab.hpp"
#include "profiles/VirginProfile.hpp"
#include "storage/HistoryStore.hpp"

#include <QTabBar>
#include <QIcon>

namespace virgin::browser {

TabManager::TabManager(virgin::profiles::VirginProfile* profile,
                       virgin::storage::HistoryStore* history,
                       QWidget* parent)
    : QTabWidget(parent), profile_(profile), history_(history)
{
    setTabsClosable(true);
    setMovable(true);
    setDocumentMode(true);
    setElideMode(Qt::ElideRight);
    tabBar()->setExpanding(false);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &QTabWidget::currentChanged, this, &TabManager::onCurrentChanged);
    connect(this, &QTabWidget::tabCloseRequested, this, &TabManager::onTabCloseRequested);
}

TabManager::~TabManager() = default;

BrowserTab* TabManager::currentTab() const {
    return qobject_cast<BrowserTab*>(currentWidget());
}

BrowserTab* TabManager::tabAt(int index) const {
    return qobject_cast<BrowserTab*>(widget(index));
}

QList<QUrl> TabManager::allTabUrls() const {
    QList<QUrl> out;
    for (int i=0;i<count();++i) {
        if (auto* t = tabAt(i)) out << t->url();
    }
    return out;
}

QList<BrowserTab*> TabManager::allTabs() const {
    QList<BrowserTab*> out;
    for (int i=0;i<count();++i) if (auto* t = tabAt(i)) out << t;
    return out;
}

void TabManager::restoreTabs(const QList<QUrl>& urls, int activeIndex) {
    // Do not keep the automatically created new tab when restoring a session.
    if (count()==1 && tabAt(0) && tabAt(0)->url().toString()=="virgin://newtab") {
        QWidget* placeholder = widget(0);
        removeTab(0);
        placeholder->deleteLater();
    }
    for (auto& u : urls) {
        createTab(u, false);
    }
    if (activeIndex>=0 && activeIndex<count()) setCurrentIndex(activeIndex);
    else if (count()>0) setCurrentIndex(0);
    if (count()==0) createTab(QUrl(), true);
}

void TabManager::closeAllTabs() {
    while (count()>0) {
        QWidget* w = widget(0);
        removeTab(0);
        w->deleteLater();
    }
}

BrowserTab* TabManager::createTab(const QUrl& url, bool makeActive) {
    auto* tab = new BrowserTab(profile_, this);
    int idx = addTab(tab, "New Tab");
    connect(tab, &BrowserTab::titleChanged, this, &TabManager::onTabTitleChanged);
    connect(tab, &BrowserTab::urlChanged, this, &TabManager::onTabUrlChanged);
    connect(tab, &BrowserTab::newTabRequested, this, [this](const QUrl& u){
        createTab(u, true);
    });

    tab->navigate(url.isEmpty() ? QUrl("virgin://newtab") : url);

    if (makeActive) setCurrentIndex(idx);
    emit tabCountChanged(count());
    return tab;
}

void TabManager::closeTab(int index) {
    if (index <0 || index>=count()) return;
    auto* tab = tabAt(index);
    if (!tab) return;
    // Push to closed stack for Ctrl+Shift+T (Sec 76)
    if (tab->url().isValid() && tab->url().scheme()!="virgin" ) {
        ClosedTab ct{ tab->url(), tab->title(), index };
        closedStack_.push(ct);
        if (closedStack_.size() > kMaxClosed) closedStack_.removeFirst();
    } else if (tab->url().toString()!="virgin://newtab" && !tab->url().isEmpty()) {
        ClosedTab ct{ tab->url(), tab->title(), index };
        closedStack_.push(ct);
        if (closedStack_.size() > kMaxClosed) closedStack_.removeFirst();
    }

    if (count() <= 1) {
        createTab(QUrl(), true);
    }
    QWidget* w = widget(index);
    removeTab(index);
    w->deleteLater();
    emit tabCountChanged(count());
}

void TabManager::closeCurrentTab() {
    closeTab(currentIndex());
}

void TabManager::duplicateTab(int index) {
    auto* tab = tabAt(index);
    if (!tab) return;
    createTab(tab->url(), true);
}

void TabManager::reopenLastClosedTab() {
    if (closedStack_.isEmpty()) return;
    ClosedTab ct = closedStack_.pop();
    auto* tab = createTab(ct.url, true);
    Q_UNUSED(tab)
    // Title will update via signal
}

void TabManager::onCurrentChanged(int index) {
    auto* tab = tabAt(index);
    emit activeTabChanged(tab);
}

void TabManager::onTabCloseRequested(int index) {
    closeTab(index);
}

void TabManager::onTabTitleChanged(const QString& title) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    if (!tab) return;
    int idx = indexOf(tab);
    if (idx != -1) {
        QString t = title.isEmpty() ? "New Tab" : title;
        if (t.length() > 32) t = t.left(32) + "…";
        setTabText(idx, t);
        setTabToolTip(idx, title);
    }
}

void TabManager::onTabUrlChanged(const QUrl& url) {
    auto* tab = qobject_cast<BrowserTab*>(sender());
    if (!tab) return;
    if (history_ && profile_ && profile_->type() == profiles::VirginProfile::Type::Normal) {
        history_->enqueueVisit(url, tab->title());
    }
    Q_UNUSED(url)
}

} // namespace virgin::browser
