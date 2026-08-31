#include "Omnibox.hpp"
#include "storage/HistoryStore.hpp"
#include "storage/BookmarkStore.hpp"
#include "security/UrlSafety.hpp"
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QStyle>

namespace virgin::ui {

Omnibox::Omnibox(virgin::storage::HistoryStore* history,
                 virgin::storage::BookmarkStore* bookmarks,
                 QWidget* parent)
    : QLineEdit(parent), history_(history), bookmarks_(bookmarks)
{
    setClearButtonEnabled(true);
    model_ = new QStringListModel(this);
    completer_ = new QCompleter(model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setFilterMode(Qt::MatchContains);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    completer_->setMaxVisibleItems(8);
    setCompleter(completer_);

    connect(this, &QLineEdit::textChanged, this, &Omnibox::updateCompletions);
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated),
            this, &Omnibox::onCompletionActivated);
    connect(this, &QLineEdit::returnPressed, [this]{
        if (completer_ && completer_->popup()->isVisible()) return;
        emit returnPressedWithText(text());
    });
}

void Omnibox::setUrl(const QUrl& url) {
    if (url.scheme() == "virgin") {
        setText(url.toString());
    } else {
        setText(url.toDisplayString());
    }
    updateSecurityIndicator(url);
    setCursorPosition(0);
}

QUrl Omnibox::url() const {
    return QUrl::fromUserInput(text());
}

void Omnibox::updateCompletions(const QString& text) {
    if (!history_ && !bookmarks_) return;
    if (text.length() < 2) {
        model_->setStringList({});
        return;
    }
    // No remote autocomplete (Sec 37) — only local history/bookmarks
    QStringList items;
    completionMap_.clear();

    // Bookmarks first (higher priority)
    if (bookmarks_) {
        auto bms = bookmarks_->search(text, 5);
        for (auto& p : bms) {
            QString display = p.second.isEmpty() ? p.first.toString() : p.second + " — " + p.first.toString();
            if (!completionMap_.contains(display)) {
                items << display;
                completionMap_.insert(display, p.first);
            }
        }
    }
    if (history_) {
        auto hist = history_->completions(text, 8);
        for (auto& p : hist) {
            QString display = p.second.isEmpty() ? p.first.toString() : p.second + " — " + p.first.toString();
            if (!completionMap_.contains(display)) {
                items << display;
                completionMap_.insert(display, p.first);
            }
        }
    }
    rebuildModel(items);
    if (!items.isEmpty()) completer_->complete();
}

void Omnibox::rebuildModel(const QStringList& items) {
    model_->setStringList(items);
}

void Omnibox::onCompletionActivated(const QString& text) {
    QUrl u = completionMap_.value(text);
    if (u.isValid()) {
        setText(u.toString());
        emit completionSelected(u);
        emit returnPressedWithText(u.toString());
    }
}

void Omnibox::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (completer_ && completer_->popup()->isVisible()) {
            completer_->popup()->hide();
            return;
        }
        clearFocus();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void Omnibox::focusInEvent(QFocusEvent* event) {
    QLineEdit::focusInEvent(event);
}

void Omnibox::updateSecurityIndicator(const QUrl& url) {
    if (url.scheme() == "https") {
        setToolTip("Secure (HTTPS) — " + security::UrlSafety::highlightRegistrableDomain(url));
        setProperty("invalid", false);
    } else if (url.scheme() == "http") {
        setToolTip("INSECURE HTTP — " + url.host());
        setProperty("invalid", true);
    } else {
        setToolTip(url.toString());
        setProperty("invalid", false);
    }
    style()->unpolish(this);
    style()->polish(this);
}

} // namespace virgin::ui
