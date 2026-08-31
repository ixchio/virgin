#pragma once

#include <QString>
#include <QStringList>
#include <QHash>
#include <QSet>
#include <QUrl>
#include <vector>

namespace virgin::adblock {

struct CosmeticRule {
    QStringList domains; // empty = global; may contain "~domain" for negation
    QString selector;
    bool isException = false; // #@#
    bool isScriptlet = false;
    QString raw;
};

class CosmeticEngine {
public:
    CosmeticEngine() = default;
    void clear();
    void addRule(const CosmeticRule& rule);
    void addRules(const std::vector<CosmeticRule>& rules, const std::vector<CosmeticRule>& exceptions);
    void build();

    // Sec 16: minimal CSS for current page domain — domain-scoped, no global bloat
    QString cssForHost(const QString& host) const;
    QString cssForUrl(const QUrl& url) const;
    QString cssForHostWithCount(const QString& host, int* outCount) const;

    size_t globalCount() const { return static_cast<size_t>(globalSelectors_.size()); }
    size_t domainCount() const { return static_cast<size_t>(domainSelectors_.size()); }
    size_t exceptionCount() const { return static_cast<size_t>(domainExceptions_.size()); }
    size_t totalSelectors() const;

private:
    bool domainMatchesHost(const QString& domain, const QString& host) const;
    bool isHostExcepted(const QString& host, const QString& selector) const;

    QStringList globalSelectors_;
    QHash<QString, QStringList> domainSelectors_; // domain -> selectors (include)
    QHash<QString, QSet<QString>> domainExceptions_; // domain -> set of excepted selectors (from #@# and ~)
    // For negation like ~example.com##.ad, we store selector globally and exception for example.com
};

} // namespace virgin::adblock
