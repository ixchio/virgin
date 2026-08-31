#include "CosmeticEngine.hpp"
#include <QSet>

namespace virgin::adblock {

void CosmeticEngine::clear() {
    globalSelectors_.clear();
    domainSelectors_.clear();
    domainExceptions_.clear();
}

void CosmeticEngine::addRule(const CosmeticRule& rule) {
    if (rule.isScriptlet) return; // Sec 17: no arbitrary scriptlets
    if (rule.selector.isEmpty()) return;
    if (rule.selector.size() > 1024) return;

    if (rule.isException) {
        // #@# : domain -> selector exception
        if (rule.domains.isEmpty()) {
            // Global exception: ## exception without domain? Removes global selector
            // Treat as global exception: add to each? For now, add to globalExceptions_ under ""
            domainExceptions_[""].insert(rule.selector);
        } else {
            for (auto& d : rule.domains) {
                QString dom = d.toLower().trimmed();
                if (dom.isEmpty()) continue;
                bool isNeg = dom.startsWith('~');
                QString clean = isNeg ? dom.mid(1) : dom;
                if (clean.isEmpty()) continue;
                // For exception, negated domain doesn't make sense; just add
                domainExceptions_[clean].insert(rule.selector);
            }
        }
        return;
    }

    // Normal hiding rule ## or #?#
    if (rule.domains.isEmpty()) {
        // Global: ##.ad
        if (!globalSelectors_.contains(rule.selector))
            globalSelectors_ << rule.selector;
        return;
    }

    // Domain-specific: may have ~ negations like ~example.com##.ad
    // Interpret: ~example.com means apply to all except example.com
    // We handle by adding to global and exception
    bool hasNeg = false;
    bool hasPos = false;
    for (auto& d : rule.domains) if (d.startsWith('~')) hasNeg = true; else hasPos = true;

    if (hasNeg && !hasPos) {
        // Only negations: ~a.com, ~b.com##.ad => global except those
        if (!globalSelectors_.contains(rule.selector))
            globalSelectors_ << rule.selector;
        for (auto& d : rule.domains) {
            if (d.startsWith('~')) {
                QString clean = d.mid(1).toLower();
                domainExceptions_[clean].insert(rule.selector);
            }
        }
    } else {
        // Mixed or only positives: add to each positive domain, and for negated, add global + exception
        for (auto& d : rule.domains) {
            if (d.startsWith('~')) {
                // For mixed, treat ~ as exception for global? But we already have positives, so just add global too
                if (!globalSelectors_.contains(rule.selector))
                    globalSelectors_ << rule.selector;
                QString clean = d.mid(1).toLower();
                domainExceptions_[clean].insert(rule.selector);
            } else {
                QString clean = d.toLower();
                auto& list = domainSelectors_[clean];
                if (!list.contains(rule.selector)) list << rule.selector;
            }
        }
    }
}

void CosmeticEngine::addRules(const std::vector<CosmeticRule>& rules, const std::vector<CosmeticRule>& exceptions) {
    for (auto& r : rules) addRule(r);
    for (auto& e : exceptions) addRule(e); // exceptions have isException true
}

void CosmeticEngine::build() {
    QSet<QString> seen;
    QStringList dedup;
    for (auto& s : globalSelectors_) if (!seen.contains(s)) { seen.insert(s); dedup << s; }
    globalSelectors_ = dedup;
    for (auto it = domainSelectors_.begin(); it != domainSelectors_.end(); ++it) {
        QSet<QString> s; QStringList d;
        for (auto& sel : it.value()) if (!s.contains(sel)) { s.insert(sel); d << sel; }
        it.value() = d;
    }
}

bool CosmeticEngine::domainMatchesHost(const QString& domain, const QString& host) const {
    if (domain.isEmpty()) return false;
    if (host == domain) return true;
    if (host.endsWith("." + domain)) return true;
    return false;
}

bool CosmeticEngine::isHostExcepted(const QString& host, const QString& selector) const {
    // Check if any exception domain matches host and contains selector
    // Need to check all exception domains that are suffix of host
    // For performance, iterate over host suffixes
    QString cur = host;
    while (!cur.isEmpty()) {
        auto it = domainExceptions_.find(cur);
        if (it != domainExceptions_.end() && it.value().contains(selector)) return true;
        const qsizetype dot = cur.indexOf('.');
        if (dot == -1) break;
        cur = cur.mid(dot + 1);
    }
    // Global exception "" ?
    auto itGlob = domainExceptions_.find("");
    if (itGlob != domainExceptions_.end() && itGlob.value().contains(selector)) return true;
    return false;
}

QString CosmeticEngine::cssForHost(const QString& host) const {
    int c = 0;
    return cssForHostWithCount(host, &c);
}

QString CosmeticEngine::cssForHostWithCount(const QString& host, int* outCount) const {
    QString h = host.toLower().trimmed();
    if (outCount) *outCount = 0;
    if (h.isEmpty()) return {};

    QSet<QString> collected;
    // Global selectors first
    for (auto& sel : globalSelectors_) {
        if (isHostExcepted(h, sel)) continue;
        collected.insert(sel);
    }
    // Domain-specific: walk host suffixes: host, parent, grandparent
    QString cur = h;
    while (!cur.isEmpty()) {
        auto it = domainSelectors_.find(cur);
        if (it != domainSelectors_.end()) {
            for (auto& sel : it.value()) {
                if (isHostExcepted(h, sel)) continue;
                collected.insert(sel);
            }
        }
        const qsizetype dot = cur.indexOf('.');
        if (dot == -1) break;
        cur = cur.mid(dot + 1);
    }

    if (collected.isEmpty()) return {};
    if (outCount) *outCount = static_cast<int>(collected.size());
    // Sec 16: minimal CSS — join selectors with comma, single rule
    QStringList list = collected.values();
    // Sort for determinism
    std::sort(list.begin(), list.end());
    // Limit to avoid massive injection: max 1000 selectors per page (Sec 16)
    if (list.size() > 1000) list = list.mid(0, 1000);
    return list.join(", ") + " { display: none !important; }";
}

QString CosmeticEngine::cssForUrl(const QUrl& url) const {
    return cssForHost(url.host());
}

size_t CosmeticEngine::totalSelectors() const {
    size_t c = static_cast<size_t>(globalSelectors_.size());
    for (auto it = domainSelectors_.constBegin(); it != domainSelectors_.constEnd(); ++it) {
        c += static_cast<size_t>(it.value().size());
    }
    return c;
}

} // namespace virgin::adblock
