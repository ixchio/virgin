#pragma once

#include <QUrl>
#include <QString>
#include <QStringList>
#include <QObject>
#include <QHash>
#include <QVector>
#include <QRegularExpression>
#include <memory>
#include <atomic>
#include <vector>
#include <shared_mutex>
#include <unordered_set>
#include <cstdint>

#include "DomainTrie.hpp"
#include "RuleIndex.hpp"
#include "AhoCorasick.hpp"
#include "CosmeticEngine.hpp"

namespace virgin::network { struct RequestContext; }

namespace virgin::adblock {

constexpr uint32_t kMaskDocument      = 1u << 0;
constexpr uint32_t kMaskSubDocument   = 1u << 1;
constexpr uint32_t kMaskScript        = 1u << 2;
constexpr uint32_t kMaskImage         = 1u << 3;
constexpr uint32_t kMaskStylesheet    = 1u << 4;
constexpr uint32_t kMaskFont          = 1u << 5;
constexpr uint32_t kMaskMedia         = 1u << 6;
constexpr uint32_t kMaskXhr           = 1u << 7;
constexpr uint32_t kMaskFetch         = 1u << 8;
constexpr uint32_t kMaskWebSocket     = 1u << 9;
constexpr uint32_t kMaskPingBeacon    = 1u << 10;
constexpr uint32_t kMaskOther         = 1u << 11;
constexpr uint32_t kMaskAll           = 0xFFFFFFFFu;

enum class RuleAction { Allow, Block };
enum class MatchKind { ExactHost, Suffix, DomainPath, Substring, Regex, StartAnchor, EndAnchor };

struct NetworkRule {
    uint32_t id = 0;
    RuleAction action = RuleAction::Block;
    uint32_t resourceMask = kMaskAll;
    MatchKind kind = MatchKind::Substring;
    QString pattern;
    QString hostAnchor;
    QRegularExpression compiledRegex;
    QString rawPattern;
    bool thirdPartyOnly = false;
    bool firstPartyOnly = false;
    bool isImportant = false;
    QStringList includeDomains;
    QStringList excludeDomains;
};

struct CompiledRules {
    std::vector<NetworkRule> blockRules;
    std::vector<NetworkRule> allowRules;
    uint64_t version = 0;

    // Read-mostly indexes built by FilterCompiler and immutable after publication.
    QHash<QString, QVector<int>> exactBlockMap;
    QHash<QString, QVector<int>> exactAllowMap;
    DomainTrie blockSuffixTrie;
    DomainTrie allowSuffixTrie;
    RuleIndex blockTokenIndex;
    RuleIndex allowTokenIndex;
    AhoCorasick blockAho;
    AhoCorasick allowAho;
    QVector<int> blockRegexIds;
    QVector<int> allowRegexIds;
    // Patterns too long for the Aho index remain correct through this bounded
    // linear fallback rather than silently becoming unreachable.
    QVector<int> blockLinearIds;
    QVector<int> allowLinearIds;
    QVector<int> importantBlockIds;

    // Domain-scoped cosmetic rules.
    std::vector<CosmeticRule> cosmeticRulesStore;
    std::vector<CosmeticRule> cosmeticExceptionsStore;
    CosmeticEngine cosmetic;

    size_t totalIndexed() const { return blockRules.size() + allowRules.size(); }
    size_t totalCosmetic() const { return cosmetic.totalSelectors(); }
};

class RuleStore final {
public:
    RuleStore() {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        rules_.store(std::make_shared<CompiledRules>());
#else
        std::shared_ptr<const CompiledRules> initial = std::make_shared<CompiledRules>();
        std::atomic_store(&rules_, std::move(initial));
#endif
    }
    std::shared_ptr<const CompiledRules> load() const {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        return rules_.load(std::memory_order_acquire);
#else
        return std::atomic_load_explicit(&rules_, std::memory_order_acquire);
#endif
    }
    void store(std::shared_ptr<const CompiledRules> rules) {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        rules_.store(std::move(rules), std::memory_order_release);
#else
        std::atomic_store_explicit(&rules_, std::move(rules), std::memory_order_release);
#endif
    }

private:
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    std::atomic<std::shared_ptr<const CompiledRules>> rules_;
#else
    // libstdc++ 11 (Ubuntu 22.04) predates std::atomic<shared_ptr<T>>.
    std::shared_ptr<const CompiledRules> rules_;
#endif
};

struct BlockResult {
    bool blocked = false;
    bool isTracker = false;
    bool isAd = false;
    uint32_t ruleId = 0;
    QString filter;
    QString domain;
};

struct SiteStats {
    uint64_t requests = 0;
    uint64_t blocked = 0;
    uint64_t trackers = 0;
    uint64_t ads = 0;
};

class AdBlockEngine final : public QObject {
    Q_OBJECT
public:
    explicit AdBlockEngine(QObject* parent = nullptr);
    AdBlockEngine(std::shared_ptr<RuleStore> ruleStore, QObject* parent);
    ~AdBlockEngine() override;

    BlockResult shouldBlock(const virgin::network::RequestContext& ctx) const;

    void updateRules(std::shared_ptr<const CompiledRules> newRules);
    std::shared_ptr<const CompiledRules> currentRules() const;

    uint64_t totalBlocked() const { return totalBlocked_.load(); }
    uint64_t trackersBlocked() const { return trackersBlocked_.load(); }
    uint64_t adsBlocked() const { return adsBlocked_.load(); }
    uint64_t totalRequests() const { return totalRequests_.load(); }
    int blockedForHost(const QString& host) const;
    SiteStats siteStatsForHost(const QString& host) const;
    QHash<QString,int> snapshotPerHost() const;

    void loadDefaultLists();
    bool isEnabled() const { return enabled_.load(std::memory_order_relaxed); }
    void setEnabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }

    // Minimal domain-scoped CSS per host.
    QString cosmeticCssForUrl(const QUrl& url) const;
    QString cosmeticCssForHost(const QString& host) const;

    // Compiled-index diagnostics.
    struct Stats {
        size_t blockRules = 0;
        size_t allowRules = 0;
        size_t suffixBlock = 0;
        size_t suffixAllow = 0;
        size_t tokenEntries = 0;
        size_t ahoNodesBlock = 0;
        size_t ahoNodesAllow = 0;
        size_t regexBlock = 0;
        size_t regexAllow = 0;
    };
    Stats compiledStats() const;

signals:
    void blocked(const virgin::adblock::BlockResult& result, const QUrl& requestUrl, const QUrl& firstParty);

private:
    bool domainMatches(const NetworkRule& r, const QUrl& firstParty) const;
    bool resourceMatches(const NetworkRule& r, const virgin::network::RequestContext& ctx) const;
    bool hostMatches(const NetworkRule& r, const QString& host, const QString& urlLower) const;
    bool ruleMatches(const NetworkRule& r, const virgin::network::RequestContext& ctx, const QString& hostLower, const QString& urlLower) const;

    // Staged matching helpers
    bool checkExact(const QHash<QString, QVector<int>>& exactMap, const std::vector<NetworkRule>& rules,
                    const virgin::network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const;
    bool checkSuffix(const DomainTrie& trie, const std::vector<NetworkRule>& rules,
                     const virgin::network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const;
    bool checkTokenAndAho(const RuleIndex& index, const AhoCorasick& aho, const std::vector<NetworkRule>& rules,
                          const virgin::network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const;
    bool checkLinear(const QVector<int>& ruleIds, const std::vector<NetworkRule>& rules,
                     const virgin::network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const;
    bool checkRegex(const QVector<int>& ruleIds, const std::vector<NetworkRule>& rules,
                    const virgin::network::RequestContext& ctx, const QString& hostLower, const QString& urlLower, BlockResult& out) const;
    bool checkImportantBlocks(const QVector<int>& importantIds,
                              const std::vector<NetworkRule>& rules,
                              const virgin::network::RequestContext& ctx,
                              const QString& hostLower,
                              const QString& urlLower,
                              BlockResult& out) const;

    std::shared_ptr<RuleStore> ruleStore_;
    mutable std::shared_mutex mutex_;
    std::atomic<uint64_t> totalBlocked_{0};
    std::atomic<uint64_t> trackersBlocked_{0};
    std::atomic<uint64_t> adsBlocked_{0};
    std::atomic<uint64_t> totalRequests_{0};
    std::atomic<bool> enabled_{true};
    QHash<QString, int> perHostBlocked_;
    QHash<QString, SiteStats> perHostStats_;
};

} // namespace virgin::adblock
