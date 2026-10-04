#include "AdBlockEngine.hpp"
#include "FilterParser.hpp"
#include "FilterCompiler.hpp"
#include "network/RequestClassifier.hpp"
#include "app/Paths.hpp"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <mutex>
#include <shared_mutex>

namespace virgin::adblock {

namespace {

bool isAbpSeparator(QChar character) {
    return !character.isLetterOrNumber() && character != QLatin1Char('_') &&
           character != QLatin1Char('-') && character != QLatin1Char('.') &&
           character != QLatin1Char('%');
}

bool matchesAbpGlob(const QString& text, const QString& pattern) {
    qsizetype textIndex = 0;
    qsizetype patternIndex = 0;
    qsizetype starIndex = -1;
    qsizetype starTextIndex = -1;

    while (textIndex < text.size()) {
        if (patternIndex == pattern.size()) return true;
        if (patternIndex < pattern.size() && pattern[patternIndex] == QLatin1Char('*')) {
            starIndex = patternIndex++;
            starTextIndex = textIndex;
            continue;
        }
        if (patternIndex < pattern.size() && pattern[patternIndex] == QLatin1Char('^')) {
            if (isAbpSeparator(text[textIndex])) {
                ++patternIndex;
                ++textIndex;
                continue;
            }
        } else if (patternIndex < pattern.size() && pattern[patternIndex] == text[textIndex]) {
            ++patternIndex;
            ++textIndex;
            continue;
        }
        if (starIndex >= 0) {
            patternIndex = starIndex + 1;
            textIndex = ++starTextIndex;
            continue;
        }
        return false;
    }

    while (patternIndex < pattern.size() && pattern[patternIndex] == QLatin1Char('*')) {
        ++patternIndex;
    }
    if (patternIndex < pattern.size() && pattern[patternIndex] == QLatin1Char('^')) {
        ++patternIndex; // ABP separator also matches end-of-URL.
    }
    return patternIndex == pattern.size();
}

QString pathAndLater(const QString& encodedUrl) {
    const qsizetype scheme = encodedUrl.indexOf(QStringLiteral("://"));
    if (scheme < 0) return encodedUrl;
    const qsizetype start = encodedUrl.indexOf(QLatin1Char('/'), scheme + 3);
    return start < 0 ? QStringLiteral("/") : encodedUrl.mid(start);
}

} // namespace

AdBlockEngine::AdBlockEngine(QObject* parent)
    : AdBlockEngine(std::make_shared<RuleStore>(), parent) {}

AdBlockEngine::AdBlockEngine(std::shared_ptr<RuleStore> ruleStore, QObject* parent)
    : QObject(parent), ruleStore_(std::move(ruleStore)) {
    if (!ruleStore_) ruleStore_ = std::make_shared<RuleStore>();
    const auto rules = ruleStore_->load();
    if (!rules || rules->totalIndexed() == 0) loadDefaultLists();
}

AdBlockEngine::~AdBlockEngine() = default;

bool AdBlockEngine::domainMatches(const NetworkRule& r, const QUrl& firstParty) const {
    QString fpHost = firstParty.host().toLower();
    if (fpHost.isEmpty()) return r.includeDomains.isEmpty() && r.excludeDomains.isEmpty();
    for (auto& ex : r.excludeDomains) if (fpHost == ex || fpHost.endsWith("." + ex)) return false;
    if (r.includeDomains.isEmpty()) return true;
    for (auto& inc : r.includeDomains) if (fpHost == inc || fpHost.endsWith("." + inc)) return true;
    return false;
}

bool AdBlockEngine::resourceMatches(const NetworkRule& r, const network::RequestContext& ctx) const {
    uint32_t mask = 0;
    switch (ctx.resourceType) {
        case network::ResourceType::Document: mask = kMaskDocument; break;
        case network::ResourceType::SubDocument: mask = kMaskSubDocument; break;
        case network::ResourceType::Script: mask = kMaskScript; break;
        case network::ResourceType::Image: mask = kMaskImage; break;
        case network::ResourceType::Stylesheet: mask = kMaskStylesheet; break;
        case network::ResourceType::Font: mask = kMaskFont; break;
        case network::ResourceType::Media: mask = kMaskMedia; break;
        case network::ResourceType::Xhr: mask = kMaskXhr; break;
        case network::ResourceType::Fetch: mask = kMaskFetch; break;
        case network::ResourceType::WebSocket: mask = kMaskWebSocket; break;
        case network::ResourceType::PingBeacon: mask = kMaskPingBeacon; break;
        default: mask = kMaskOther; break;
    }
    return (r.resourceMask & mask) != 0;
}

bool AdBlockEngine::hostMatches(const NetworkRule& r, const QString& host, const QString& urlLower) const {
    switch (r.kind) {
        case MatchKind::Suffix: return host == r.pattern || host.endsWith("." + r.pattern);
        case MatchKind::DomainPath:
            return (host == r.hostAnchor || host.endsWith("." + r.hostAnchor)) &&
                   matchesAbpGlob(pathAndLater(urlLower), r.pattern);
        case MatchKind::ExactHost: return host == r.pattern;
        case MatchKind::StartAnchor: return urlLower.startsWith(r.pattern);
        case MatchKind::EndAnchor: return urlLower.endsWith(r.pattern);
        case MatchKind::Regex: {
            return r.compiledRegex.isValid() && r.compiledRegex.match(urlLower).hasMatch();
        }
        case MatchKind::Substring:
        default: return urlLower.contains(r.pattern);
    }
}

bool AdBlockEngine::ruleMatches(const NetworkRule& r, const network::RequestContext& ctx, const QString& hostLower, const QString& urlLower) const {
    if (r.thirdPartyOnly && !ctx.thirdParty) return false;
    if (r.firstPartyOnly && ctx.thirdParty) return false;
    if (!domainMatches(r, ctx.firstPartyUrl)) return false;
    if (!resourceMatches(r, ctx)) return false;
    if (!hostMatches(r, hostLower, urlLower)) return false;
    return true;
}

bool AdBlockEngine::checkExact(const QHash<QString, QVector<int>>& exactMap, const std::vector<NetworkRule>& rules,
                               const network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const {
    auto it = exactMap.find(hostLower);
    if (it == exactMap.end()) return false;
    for (int idx : it.value()) {
        if (idx < 0 || static_cast<size_t>(idx) >= rules.size()) continue;
        const auto& r = rules[static_cast<size_t>(idx)];
        if (ruleMatches(r, ctx, hostLower, urlLower)) {
            out = {r.action == RuleAction::Block, false, false, r.id, r.rawPattern, hostLower};
            return true;
        }
    }
    return false;
}

bool AdBlockEngine::checkSuffix(const DomainTrie& trie, const std::vector<NetworkRule>& rules,
                                const network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const {
    auto ids = trie.findMatches(hostLower);
    for (int idx : ids) {
        if (idx < 0 || static_cast<size_t>(idx) >= rules.size()) continue;
        const auto& r = rules[static_cast<size_t>(idx)];
        if (!ruleMatches(r, ctx, hostLower, urlLower)) continue;
        out = {r.action == RuleAction::Block, false, false, r.id, r.rawPattern, hostLower};
        return true;
    }
    return false;
}

bool AdBlockEngine::checkTokenAndAho(const RuleIndex& index, const AhoCorasick& aho, const std::vector<NetworkRule>& rules,
                                     const network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const {
    // Aho-Corasick returns substring candidates, then full constraints verify each rule.
    Q_UNUSED(index)
    if (aho.empty()) return false;
    auto cands = aho.search(urlLower);
    // Deduplicate and limit: only first 16 candidates to keep p95 low; worst-case still checks regex later
    if (cands.empty()) return false;
    // For performance, sort and limit
    // Note: Aho may return many ids; we iterate and early exit on first matching rule that passes full constraints
    for (int idx : cands) {
        if (idx < 0 || static_cast<size_t>(idx) >= rules.size()) continue;
        const auto& r = rules[static_cast<size_t>(idx)];
        // For StartAnchor/EndAnchor, need hostMatches to verify anchor (Aho only checks contains)
        if (!ruleMatches(r, ctx, hostLower, urlLower)) continue;
        out = {r.action == RuleAction::Block, false, false, r.id, r.rawPattern, hostLower};
        return true;
    }
    return false;
}

bool AdBlockEngine::checkLinear(const QVector<int>& ruleIds, const std::vector<NetworkRule>& rules,
                                const network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const {
    for (int idx : ruleIds) {
        if (idx < 0 || static_cast<size_t>(idx) >= rules.size()) continue;
        const auto& r = rules[static_cast<size_t>(idx)];
        if (!ruleMatches(r, ctx, hostLower, urlLower)) continue;
        out = {r.action == RuleAction::Block, false, false, r.id, r.rawPattern, hostLower};
        return true;
    }
    return false;
}

bool AdBlockEngine::checkRegex(const QVector<int>& ruleIds, const std::vector<NetworkRule>& rules,
                               const network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const {
    // Regex rules cannot safely use exact-token indexing: a token is only a
    // hint, while matching is governed by the complete regular expression.
    for (int idx : ruleIds) {
        if (idx < 0 || static_cast<size_t>(idx) >= rules.size()) continue;
        const auto& r = rules[static_cast<size_t>(idx)];
        if (!ruleMatches(r, ctx, hostLower, urlLower)) continue;
        out = {r.action == RuleAction::Block, false, false, r.id, r.rawPattern, hostLower};
        return true;
    }
    return false;
}

bool AdBlockEngine::checkImportantBlocks(const QVector<int>& importantIds,
                                         const std::vector<NetworkRule>& rules,
                                         const network::RequestContext& ctx,
                                         const QString& hostLower,
                                         const QString& urlLower,
                                         BlockResult& out) const {
    for (const int index : importantIds) {
        if (index < 0 || static_cast<size_t>(index) >= rules.size()) continue;
        const auto& rule = rules[static_cast<size_t>(index)];
        if (!ruleMatches(rule, ctx, hostLower, urlLower)) continue;
        out = {true, false, false, rule.id, rule.rawPattern, hostLower};
        return true;
    }
    return false;
}

BlockResult AdBlockEngine::shouldBlock(const network::RequestContext& ctx) const {
    if (!enabled_.load(std::memory_order_relaxed)) return {};
    auto rules = ruleStore_->load();
    if (!rules) return {};

    const QString urlLower = ctx.requestUrl.toString(QUrl::FullyEncoded).toLower();
    const QString host = ctx.requestUrl.host().toLower();
    QString fpHost = ctx.firstPartyUrl.host().toLower();
    if (fpHost.isEmpty()) fpHost = host;

    const_cast<AdBlockEngine*>(this)->totalRequests_.fetch_add(1, std::memory_order_relaxed);
    if (!fpHost.isEmpty()) {
        auto* self = const_cast<AdBlockEngine*>(this);
        std::unique_lock<std::shared_mutex> lock(self->mutex_);
        self->perHostStats_[fpHost].requests++;
    }
    if (rules->blockRules.empty() && rules->allowRules.empty()) return {};

    // ABP/uBO $important block rules override ordinary exception rules.
    BlockResult blockRes;
    bool matched = checkImportantBlocks(rules->importantBlockIds, rules->blockRules,
                                        ctx, host, urlLower, blockRes);

    // --- Allow (exception) priority: staged check for allowRules ---
    if (!matched) {
        BlockResult allowRes;
        // Stage 1 exact allow
        if (checkExact(rules->exactAllowMap, rules->allowRules, ctx, host, urlLower, allowRes)) {
            // Exception matched -> allow (not blocked)
            return allowRes;
        }
        // Stage 2 suffix allow
        if (checkSuffix(rules->allowSuffixTrie, rules->allowRules, ctx, host, urlLower, allowRes)) {
            return allowRes;
        }
        // Stage 4 Aho allow (substring)
        if (checkTokenAndAho(rules->allowTokenIndex, rules->allowAho, rules->allowRules, ctx, host, urlLower, allowRes)) {
            return allowRes;
        }
        // Stage 5 long-pattern allow fallback
        if (checkLinear(rules->allowLinearIds, rules->allowRules, ctx, host, urlLower, allowRes)) {
            return allowRes;
        }
        // Stage 6 regex allow
        if (checkRegex(rules->allowRegexIds, rules->allowRules, ctx, host, urlLower, allowRes)) {
            return allowRes;
        }
    }

    // --- Block path: staged check for blockRules ---
    // Stage 1 exact
    if (matched) {
        // Important rule already selected.
    } else if (checkExact(rules->exactBlockMap, rules->blockRules, ctx, host, urlLower, blockRes)) matched = true;
    // Stage 2 suffix trie
    else if (checkSuffix(rules->blockSuffixTrie, rules->blockRules, ctx, host, urlLower, blockRes)) matched = true;
    // Stage 4 Aho substring index
    else if (checkTokenAndAho(rules->blockTokenIndex, rules->blockAho, rules->blockRules, ctx, host, urlLower, blockRes)) matched = true;
    // Stage 5 long-pattern fallback
    else if (checkLinear(rules->blockLinearIds, rules->blockRules, ctx, host, urlLower, blockRes)) matched = true;
    // Stage 6 regex slow path
    else if (checkRegex(rules->blockRegexIds, rules->blockRules, ctx, host, urlLower, blockRes)) matched = true;

    if (!matched) return {};

    // Matched block rule — update stats
    bool isTracker = urlLower.contains("analytics") || urlLower.contains("tracker") ||
                     urlLower.contains("pixel") || urlLower.contains("collect") ||
                     host.contains("doubleclick") || host.contains("googletagmanager");
    bool isAd = !isTracker;

    const_cast<AdBlockEngine*>(this)->totalBlocked_.fetch_add(1, std::memory_order_relaxed);
    if (isTracker) const_cast<AdBlockEngine*>(this)->trackersBlocked_.fetch_add(1, std::memory_order_relaxed);
    else const_cast<AdBlockEngine*>(this)->adsBlocked_.fetch_add(1, std::memory_order_relaxed);

    if (!fpHost.isEmpty()) {
        auto* self = const_cast<AdBlockEngine*>(this);
        std::unique_lock<std::shared_mutex> lock(self->mutex_);
        self->perHostBlocked_[fpHost]++;
        auto& site = self->perHostStats_[fpHost];
        site.blocked++;
        if (isTracker) site.trackers++;
        if (isAd) site.ads++;
    }

    BlockResult res{true, isTracker, isAd, blockRes.ruleId, blockRes.filter, host};
    QMetaObject::invokeMethod(const_cast<AdBlockEngine*>(this), [self=const_cast<AdBlockEngine*>(this), res, ctx]{
        emit self->blocked(res, ctx.requestUrl, ctx.firstPartyUrl);
    }, Qt::QueuedConnection);
    return res;
}

int AdBlockEngine::blockedForHost(const QString& host) const {
    std::shared_lock<std::shared_mutex> lock(const_cast<AdBlockEngine*>(this)->mutex_);
    return perHostBlocked_.value(host.toLower(), 0);
}

SiteStats AdBlockEngine::siteStatsForHost(const QString& host) const {
    std::shared_lock<std::shared_mutex> lock(const_cast<AdBlockEngine*>(this)->mutex_);
    return perHostStats_.value(host.toLower());
}

QHash<QString,int> AdBlockEngine::snapshotPerHost() const {
    std::shared_lock<std::shared_mutex> lock(const_cast<AdBlockEngine*>(this)->mutex_);
    return perHostBlocked_;
}

void AdBlockEngine::updateRules(std::shared_ptr<const CompiledRules> newRules) {
    if (newRules) ruleStore_->store(std::move(newRules));
}

std::shared_ptr<const CompiledRules> AdBlockEngine::currentRules() const {
    return ruleStore_->load();
}

AdBlockEngine::Stats AdBlockEngine::compiledStats() const {
    auto r = ruleStore_->load();
    if (!r) return {};
    Stats s;
    s.blockRules = r->blockRules.size();
    s.allowRules = r->allowRules.size();
    s.suffixBlock = r->blockSuffixTrie.size();
    s.suffixAllow = r->allowSuffixTrie.size();
    s.tokenEntries = r->blockTokenIndex.tokenCount() + r->allowTokenIndex.tokenCount();
    s.ahoNodesBlock = r->blockAho.nodeCount();
    s.ahoNodesAllow = r->allowAho.nodeCount();
    s.regexBlock = static_cast<size_t>(r->blockRegexIds.size());
    s.regexAllow = static_cast<size_t>(r->allowRegexIds.size());
    return s;
}

QString AdBlockEngine::cosmeticCssForHost(const QString& host) const {
    auto r = ruleStore_->load();
    if (!r) return {};
    return r->cosmetic.cssForHost(host);
}

QString AdBlockEngine::cosmeticCssForUrl(const QUrl& url) const {
    auto r = ruleStore_->load();
    if (!r) return {};
    return r->cosmetic.cssForUrl(url);
}

void AdBlockEngine::loadDefaultLists() {
    QStringList existing;
    const QString maintainedRules = app::Paths::builtinFiltersDir() +
        QStringLiteral("/virgin-protection.txt");
    const QDir downloadedDirectory(app::Paths::filtersDir());
    const QFileInfoList subscriptions = downloadedDirectory.entryInfoList(
        {QStringLiteral("subscription-*.txt")}, QDir::Files, QDir::Name);
    for (const QFileInfo& subscription : subscriptions) {
        if (subscription.size() > 0) existing << subscription.absoluteFilePath();
    }
    if (existing.isEmpty()) {
        const QStringList candidates = {
            app::Paths::filtersDir() + "/easylist.txt",
            app::Paths::filtersDir() + "/easyprivacy.txt",
            app::Paths::builtinFiltersDir() + "/easylist.txt",
            app::Paths::builtinFiltersDir() + "/easyprivacy.txt"
        };
        for (const QString& path : candidates) if (QFile::exists(path)) existing << path;
    }
    if (QFile::exists(maintainedRules) && !existing.contains(maintainedRules)) {
        existing << maintainedRules;
    }
    if (existing.isEmpty()) return;

    // Use the cache only when it is at least as new as every source file.
    QString cachePath = app::Paths::compiledFiltersDir() + "/rules.bin";
    bool cacheFresh = QFile::exists(cachePath);
    const QDateTime cacheTime = QFileInfo(cachePath).lastModified();
    for (const QString& source : existing) {
        if (QFileInfo(source).lastModified() > cacheTime) cacheFresh = false;
    }
    if (cacheFresh) {
        auto cached = FilterCompiler::readCache(cachePath);
        if (cached && cached->totalIndexed() > 0) {
            updateRules(cached);
            return;
        }
    }

    auto compiled = FilterCompiler::compileFiles(existing);
    if (compiled && compiled->totalIndexed() > 0) {
        updateRules(compiled);
        QDir().mkpath(app::Paths::compiledFiltersDir());
        FilterCompiler::writeCache(*compiled, cachePath);
    }
}

} // namespace virgin::adblock
