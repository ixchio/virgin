#include "FilterParser.hpp"
#include <QRegularExpression>
#include <QSet>

namespace virgin::adblock {

namespace {

QString abpPatternToRegex(const QString& source, bool domainAnchor,
                          bool startAnchor, bool endAnchor) {
    QString regex;
    regex.reserve(source.size() * 2);
    if (domainAnchor) {
        // ABP || anchors at a hostname label boundary, not at an arbitrary
        // substring such as "notexample.com".
        regex += QStringLiteral("^[a-z][a-z0-9+.-]*://(?:[^/?#]*\\.)?");
    } else if (startAnchor) {
        regex += QLatin1Char('^');
    }

    static const QString kRegexMeta = QStringLiteral("\\.[]{}()+?$|");
    for (const QChar c : source) {
        if (c == QLatin1Char('*')) {
            regex += QStringLiteral(".*");
        } else if (c == QLatin1Char('^')) {
            regex += QStringLiteral("(?:[^A-Za-z0-9_.%-]|$)");
        } else {
            if (kRegexMeta.contains(c)) regex += QLatin1Char('\\');
            regex += c;
        }
    }
    if (endAnchor) regex += QLatin1Char('$');
    return regex;
}

bool isSupportedOption(const QString& option) {
    QString token = option.trimmed().toLower();
    if (token.startsWith(QLatin1Char('~'))) token.remove(0, 1);
    if (token.startsWith(QStringLiteral("domain="))) return true;
    static const QSet<QString> supported = {
        QStringLiteral("third-party"), QStringLiteral("first-party"),
        QStringLiteral("script"), QStringLiteral("image"),
        QStringLiteral("stylesheet"), QStringLiteral("css"),
        QStringLiteral("font"), QStringLiteral("font-resource"),
        QStringLiteral("media"), QStringLiteral("object"),
        QStringLiteral("object-subrequest"), QStringLiteral("xmlhttprequest"),
        QStringLiteral("xhr"), QStringLiteral("fetch"),
        QStringLiteral("websocket"), QStringLiteral("ping"),
        QStringLiteral("beacon"), QStringLiteral("document"),
        QStringLiteral("main_frame"), QStringLiteral("subdocument"),
        QStringLiteral("sub_frame"), QStringLiteral("other"),
        QStringLiteral("important")
    };
    return supported.contains(token);
}

} // namespace

bool FilterParser::isComment(const QString& line) {
    QString t = line.trimmed();
    return t.isEmpty() || t.startsWith('!') || t.startsWith('[');
}

bool FilterParser::isCosmetic(const QString& line) {
    QString t = line.trimmed();
    if (t.contains("##") || t.contains("#@#") || t.contains("#?#") || t.contains("#$#")) return true;
    return false;
}

std::optional<CosmeticRule> FilterParser::parseCosmeticRule(const QString& line) {
    QString t = line.trimmed();
    if (t.isEmpty() || isComment(t)) return std::nullopt;
    // Sec 41 bounds
    if (t.size() > 8192) return std::nullopt;
    // Scriptlets are rejected; filter data never executes arbitrary JavaScript.
    if (t.contains("##+js") || t.contains("#@#+js") || t.contains("##^js") || t.contains("$#")) return std::nullopt;

    bool isException = false;
    QString sep;
    qsizetype pos = -1;
    if ((pos = t.indexOf("#@#")) != -1) {
        isException = true;
        sep = "#@#";
    } else if ((pos = t.indexOf("##")) != -1) {
        isException = false;
        sep = "##";
    } else if (t.contains("#?#")) {
        // Extended CSS requires procedural selector evaluation. Feeding it to
        // querySelector/CSS would be incorrect, so fail closed and skip it.
        return std::nullopt;
    } else {
        return std::nullopt;
    }

    QString domainPart = t.left(pos).trimmed();
    QString selector = t.mid(pos + sep.length()).trimmed();
    if (selector.isEmpty()) return std::nullopt;
    // Sec 41: selector length limit
    if (selector.size() > 1024) return std::nullopt;
    // Reject selectors that look like script: contains "javascript:" or "<script"
    if (selector.contains("javascript:", Qt::CaseInsensitive) || selector.contains("<script", Qt::CaseInsensitive)) return std::nullopt;

    CosmeticRule r;
    r.isException = isException;
    r.isScriptlet = false;
    r.selector = selector;
    r.raw = line;

    if (domainPart.isEmpty()) {
        // Global rule: ##selector
        r.domains = QStringList();
    } else {
        // Domain list comma separated, may have ~ negations
        auto parts = domainPart.split(',', Qt::SkipEmptyParts);
        for (auto& p : parts) {
            QString d = p.trimmed().toLower();
            if (d.isEmpty()) continue;
            // Validate domain: no spaces, no */^
            if (d.contains(' ') || d.contains('*') || d.contains('^')) continue;
            r.domains << d;
        }
        // If domainPart was not empty but we filtered all, treat as global? No, skip
        if (r.domains.isEmpty() && !domainPart.isEmpty()) {
            // Could be malformed, skip
            return std::nullopt;
        }
    }
    return r;
}

uint32_t FilterParser::parseResourceMask(const QString& options) {
    if (options.isEmpty()) return kMaskAll;
    uint32_t included = 0;
    uint32_t excluded = 0;
    auto tokens = options.split(',', Qt::SkipEmptyParts);
    for (auto& tok : tokens) {
        QString t = tok.trimmed().toLower();
        const bool neg = t.startsWith('~');
        if (neg) t = t.mid(1);
        uint32_t bit = 0;
        if (t == "script") bit = kMaskScript;
        else if (t == "image") bit = kMaskImage;
        else if (t == "stylesheet" || t == "css") bit = kMaskStylesheet;
        else if (t == "font" || t == "font-resource") bit = kMaskFont;
        else if (t == "media" || t == "object" || t == "object-subrequest") bit = kMaskMedia;
        else if (t == "xmlhttprequest" || t == "xhr") bit = kMaskXhr;
        else if (t == "fetch") bit = kMaskFetch;
        else if (t == "websocket") bit = kMaskWebSocket;
        else if (t == "ping" || t == "beacon") bit = kMaskPingBeacon;
        else if (t == "document" || t == "main_frame") bit = kMaskDocument;
        else if (t == "subdocument" || t == "sub_frame") bit = kMaskSubDocument;
        else if (t == "other") bit = kMaskOther;
        if (bit == 0) continue;
        if (neg) excluded |= bit;
        else included |= bit;
    }
    const uint32_t base = included == 0 ? kMaskAll : included;
    return base & ~excluded;
}

void FilterParser::parseDomainOption(const QString& value, NetworkRule& rule) {
    auto parts = value.split('|', Qt::SkipEmptyParts);
    for (auto& p : parts) {
        QString d = p.trimmed().toLower();
        if (d.isEmpty()) continue;
        if (d.startsWith('~')) rule.excludeDomains << d.mid(1);
        else rule.includeDomains << d;
    }
}

QString FilterParser::normalizePattern(QString pat, MatchKind& kind, QString* hostAnchor) {
    kind = MatchKind::Substring;
    if (hostAnchor) hostAnchor->clear();
    if (pat.size() >= 2 && pat.startsWith('/') && pat.endsWith('/')) {
        kind = MatchKind::Regex;
        return pat.mid(1, pat.size()-2);
    }

    bool domainAnchor = false;
    bool startAnchor = false;
    bool endAnchor = false;
    if (pat.startsWith("||")) {
        domainAnchor = true;
        pat = pat.mid(2);
    } else if (pat.startsWith("|")) {
        kind = MatchKind::StartAnchor;
        pat = pat.mid(1);
        startAnchor = true;
    }
    if (pat.endsWith("|")) {
        if (kind == MatchKind::Substring) kind = MatchKind::EndAnchor;
        pat.chop(1);
        endAnchor = true;
    }

    // The overwhelmingly common ||tracker.example^ rule can use the fast,
    // label-aware domain trie. Host-anchored paths and wildcard patterns must
    // keep their full ABP semantics and therefore use the regex slow path.
    QString domainCandidate = pat;
    if (domainCandidate.endsWith(QLatin1Char('^'))) domainCandidate.chop(1);
    static const QRegularExpression hostname(
        QStringLiteral(R"(^[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$)"));
    if (domainAnchor && hostname.match(domainCandidate).hasMatch()) {
        kind = MatchKind::Suffix;
        return domainCandidate.toLower();
    }

    const qsizetype slash = domainAnchor ? pat.indexOf(QLatin1Char('/')) : -1;
    if (slash > 0 && !endAnchor) {
        const QString domain = pat.left(slash);
        if (hostname.match(domain).hasMatch()) {
            kind = MatchKind::DomainPath;
            if (hostAnchor) *hostAnchor = domain.toLower();
            return pat.mid(slash).toLower();
        }
    }

    if (domainAnchor || (startAnchor && endAnchor) ||
        pat.contains(QLatin1Char('*')) || pat.contains(QLatin1Char('^'))) {
        kind = MatchKind::Regex;
        return abpPatternToRegex(pat, domainAnchor, startAnchor, endAnchor);
    }
    if (startAnchor) kind = MatchKind::StartAnchor;
    else if (endAnchor) kind = MatchKind::EndAnchor;
    return pat.trimmed();
}

std::optional<NetworkRule> FilterParser::parseNetworkRule(const QString& line, uint32_t id) {
    QString t = line.trimmed();
    if (t.isEmpty() || isComment(t) || isCosmetic(t)) return std::nullopt;
    if (t.size() > 8192) return std::nullopt;
    if (t.size() > 0 && t[0] == '[') return std::nullopt;
    NetworkRule r;
    r.id = id;
    r.rawPattern = t;
    if (t.startsWith("@@")) {
        r.action = RuleAction::Allow;
        t = t.mid(2);
    } else {
        r.action = RuleAction::Block;
    }
    QString patternPart;
    QString optionsPart;
    const qsizetype dollar = t.lastIndexOf('$');
    if (dollar != -1) {
        QString after = t.mid(dollar+1).toLower();
        bool looksLikeOptions = after.contains("third-party") || after.contains("domain=") ||
                                after.contains("script") || after.contains("image") ||
                                after.contains("stylesheet") || after.contains("xmlhttprequest") ||
                                after.contains("document") || after.contains("important") ||
                                after.contains("first-party") || after.contains("ping") ||
                                after.contains("font") || after.contains("media") ||
                                after.contains("subdocument") || after.contains("fetch") ||
                                after.contains("other") || after.contains("websocket");
        if (looksLikeOptions || (!after.contains('/') && !after.contains('*') && !after.contains('^'))) {
            if (looksLikeOptions) {
                patternPart = t.left(dollar);
                optionsPart = t.mid(dollar+1);
            } else {
                patternPart = t;
                optionsPart.clear();
            }
        } else {
            patternPart = t;
            optionsPart.clear();
        }
    } else {
        patternPart = t;
    }
    if (patternPart.isEmpty()) return std::nullopt;
    if (isCosmetic(patternPart)) return std::nullopt;
    if (!optionsPart.isEmpty()) {
        auto opts = optionsPart.toLower();
        r.resourceMask = parseResourceMask(opts);
        auto tokens = opts.split(',', Qt::SkipEmptyParts);
        for (auto& tok : tokens) {
            QString token = tok.trimmed();
            if (!isSupportedOption(token)) return std::nullopt;
            if (token == "third-party") r.thirdPartyOnly = true;
            else if (token == "~third-party" || token == "first-party" || token == "~first-party") {
                if (token == "~third-party") r.firstPartyOnly = true;
                else if (token == "first-party") r.firstPartyOnly = true;
            } else if (token.startsWith("domain=")) {
                QString domVal = token.mid(7);
                parseDomainOption(domVal, r);
            } else if (token == "important") {
                r.isImportant = true;
            }
        }
    } else {
        r.resourceMask = kMaskAll;
    }
    QString norm = normalizePattern(patternPart, r.kind, &r.hostAnchor);
    if (norm.isEmpty()) return std::nullopt;
    if (r.kind == MatchKind::Suffix) {
        norm = norm.toLower();
        if (norm.isEmpty()) return std::nullopt;
    } else if (r.kind == MatchKind::Regex) {
        if (norm.size() > 4096) return std::nullopt;
        const QRegularExpression expression(norm, QRegularExpression::CaseInsensitiveOption);
        if (!expression.isValid()) return std::nullopt;
        r.pattern = norm;
        r.compiledRegex = expression;
        return r;
    } else {
        norm = norm.toLower();
    }
    r.pattern = norm;
    return r;
}

FilterParser::ParseResult FilterParser::parseLine(const QString& line) {
    // Try cosmetic first
    if (isCosmetic(line)) {
        auto c = parseCosmeticRule(line);
        ParseResult res;
        if (c) {
            if (c->isException) res.cosmeticExceptions.push_back(*c);
            else res.cosmeticRules.push_back(*c);
        } else {
            res.cosmeticSkipped++;
        }
        return res;
    }
    ParseResult res;
    auto r = parseNetworkRule(line, 1);
    if (r) {
        if (r->action == RuleAction::Allow) res.allowRules.push_back(*r);
        else res.blockRules.push_back(*r);
    } else {
        if (!isComment(line) && !line.trimmed().isEmpty() && !line.trimmed().startsWith("[")) {
            if (!isCosmetic(line)) res.errors++;
        } else {
            res.ignored++;
        }
    }
    return res;
}

FilterParser::ParseResult FilterParser::parseList(const QString& raw) {
    ParseResult res;
    auto lines = raw.split('\n');
    uint32_t id = 1;
    for (auto& l : lines) {
        QString t = l.trimmed();
        if (t.isEmpty() || isComment(t)) { res.ignored++; continue; }
        if (isCosmetic(t)) {
            auto c = parseCosmeticRule(t);
            if (c) {
                if (c->isException) res.cosmeticExceptions.push_back(*c);
                else res.cosmeticRules.push_back(*c);
            } else {
                res.cosmeticSkipped++;
            }
            continue;
        }
        if (res.blockRules.size() + res.allowRules.size() > 500000) break;
        auto r = parseNetworkRule(l, id);
        if (r) {
            if (r->action == RuleAction::Allow) res.allowRules.push_back(*r);
            else res.blockRules.push_back(*r);
            id++;
        } else {
            res.ignored++;
        }
    }
    return res;
}

} // namespace virgin::adblock
