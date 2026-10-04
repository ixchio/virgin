#include "FilterCompiler.hpp"
#include "FilterParser.hpp"
#include <QFile>
#include <QDataStream>
#include <QDir>
#include <QSaveFile>

namespace virgin::adblock {

namespace {
constexpr quint32 kCacheMagic = 0x5646524EU; // "VFRN"
constexpr quint32 kCacheFormat = 4U;
constexpr quint64 kMaxNetworkRules = 500'000U;
constexpr quint64 kMaxCosmeticRules = 500'000U;
constexpr qint64 kMaxCacheBytes = 64 * 1024 * 1024;

bool isAhoMatchKind(MatchKind kind) {
    return kind == MatchKind::Substring || kind == MatchKind::StartAnchor ||
           kind == MatchKind::EndAnchor;
}

void addAhoOrLinearFallback(const NetworkRule& rule, int id, AhoCorasick& aho,
                            QVector<int>& linearIds) {
    if (!isAhoMatchKind(rule.kind) || rule.pattern.isEmpty()) return;
    if (rule.pattern.size() <= 256) {
        aho.addPattern(rule.pattern.toLower(), id);
    } else {
        linearIds.append(id);
    }
}
}

std::shared_ptr<CompiledRules> FilterCompiler::compile(const QString& raw) {
    auto parsed = FilterParser::parseList(raw);
    auto compiled = std::make_shared<CompiledRules>();
    compiled->blockRules = std::move(parsed.blockRules);
    compiled->allowRules = std::move(parsed.allowRules);
    compiled->version = 1;

    // Build the suffix trie and exact-host map.
    for (size_t i = 0; i < compiled->blockRules.size(); ++i) {
        const auto& r = compiled->blockRules[i];
        if (r.isImportant) compiled->importantBlockIds.append(static_cast<int>(i));
        if (r.kind == MatchKind::Suffix) {
            compiled->blockSuffixTrie.insert(r.pattern, (int)i);
            // Also index exact host for Stage 1 if pattern is host without wildcards
            if (!r.pattern.contains('*') && !r.pattern.contains('/')) {
                compiled->exactBlockMap[r.pattern].append((int)i);
            }
        } else if (r.kind == MatchKind::DomainPath) {
            compiled->blockSuffixTrie.insert(r.hostAnchor, static_cast<int>(i));
        } else if (r.kind == MatchKind::ExactHost) {
            compiled->exactBlockMap[r.pattern].append((int)i);
        } else if (r.kind == MatchKind::Regex) {
            compiled->blockRegexIds.append((int)i);
        }
        // Substring/anchor will be handled via Aho + token index
    }
    for (size_t i = 0; i < compiled->allowRules.size(); ++i) {
        const auto& r = compiled->allowRules[i];
        if (r.kind == MatchKind::Suffix) {
            compiled->allowSuffixTrie.insert(r.pattern, (int)i);
            if (!r.pattern.contains('*') && !r.pattern.contains('/')) {
                compiled->exactAllowMap[r.pattern].append((int)i);
            }
        } else if (r.kind == MatchKind::DomainPath) {
            compiled->allowSuffixTrie.insert(r.hostAnchor, static_cast<int>(i));
        } else if (r.kind == MatchKind::ExactHost) {
            compiled->exactAllowMap[r.pattern].append((int)i);
        } else if (r.kind == MatchKind::Regex) {
            compiled->allowRegexIds.append((int)i);
        }
    }

    // Stage 3: token index
    compiled->blockTokenIndex.build(compiled->blockRules);
    compiled->allowTokenIndex.build(compiled->allowRules);

    // Stage 4: Aho-Corasick for substring patterns (include all non-suffix, non-regex)
    for (size_t i = 0; i < compiled->blockRules.size(); ++i) {
        const auto& r = compiled->blockRules[i];
        addAhoOrLinearFallback(r, static_cast<int>(i), compiled->blockAho,
                               compiled->blockLinearIds);
    }
    for (size_t i = 0; i < compiled->allowRules.size(); ++i) {
        const auto& r = compiled->allowRules[i];
        addAhoOrLinearFallback(r, static_cast<int>(i), compiled->allowAho,
                               compiled->allowLinearIds);
    }
    compiled->blockAho.build();
    compiled->allowAho.build();

    // Build domain-scoped cosmetic rules.
    compiled->cosmeticRulesStore = parsed.cosmeticRules;
    compiled->cosmeticExceptionsStore = parsed.cosmeticExceptions;
    compiled->cosmetic.clear();
    for (auto& cr : parsed.cosmeticRules) compiled->cosmetic.addRule(cr);
    for (auto& ce : parsed.cosmeticExceptions) compiled->cosmetic.addRule(ce);
    compiled->cosmetic.build();

    return compiled;
}

std::shared_ptr<CompiledRules> FilterCompiler::compileFiles(const QStringList& paths) {
    QString combined;
    combined.reserve(8 * 1024 * 1024);
    for (const auto& p : paths) {
        QFile f(p);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            // Sec 41: size limit 10MB per file
            QByteArray data = f.read(10 * 1024 * 1024);
            combined += QString::fromUtf8(data);
            combined += "\n";
        }
    }
    return compile(combined);
}

bool FilterCompiler::writeCache(const CompiledRules& rules, const QString& path) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&f);
    // Keep the on-disk cache readable by the portable Qt 6.2 AppImage build.
    out.setVersion(QDataStream::Qt_6_2);
    out << kCacheMagic << kCacheFormat;
    out << (quint64)rules.version;
    out << (quint64)rules.blockRules.size();
    out << (quint64)rules.allowRules.size();
    for (const auto& r : rules.blockRules) {
        out << (quint32)r.id << (qint32)r.action << (quint32)r.resourceMask << (qint32)r.kind
            << r.pattern << r.hostAnchor << r.rawPattern << r.thirdPartyOnly << r.firstPartyOnly << r.isImportant
            << r.includeDomains << r.excludeDomains;
    }
    for (const auto& r : rules.allowRules) {
        out << (quint32)r.id << (qint32)r.action << (quint32)r.resourceMask << (qint32)r.kind
            << r.pattern << r.hostAnchor << r.rawPattern << r.thirdPartyOnly << r.firstPartyOnly << r.isImportant
            << r.includeDomains << r.excludeDomains;
    }
    // Store cosmetic source rules so the engine can be rebuilt from cache.
    out << (quint64)rules.cosmeticRulesStore.size();
    for (auto& cr : rules.cosmeticRulesStore) {
        out << cr.domains << cr.selector << cr.isException << cr.isScriptlet << cr.raw;
    }
    out << (quint64)rules.cosmeticExceptionsStore.size();
    for (auto& cr : rules.cosmeticExceptionsStore) {
        out << cr.domains << cr.selector << cr.isException << cr.isScriptlet << cr.raw;
    }
    if (out.status() != QDataStream::Ok) return false;
    return f.commit();
}

std::shared_ptr<CompiledRules> FilterCompiler::readCache(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() <= 0 || f.size() > kMaxCacheBytes) return nullptr;
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_6_2);
    quint32 magic = 0;
    quint32 format = 0;
    in >> magic >> format;
    if (magic != kCacheMagic || format != kCacheFormat) return nullptr;
    quint64 version, blockCount, allowCount;
    in >> version >> blockCount >> allowCount;
    if (in.status() != QDataStream::Ok || blockCount > kMaxNetworkRules ||
        allowCount > kMaxNetworkRules || blockCount + allowCount > kMaxNetworkRules) return nullptr;
    auto compiled = std::make_shared<CompiledRules>();
    compiled->version = version;
    compiled->blockRules.reserve(blockCount);
    compiled->allowRules.reserve(allowCount);
    for (quint64 i = 0; i < blockCount; ++i) {
        NetworkRule r;
        quint32 id; qint32 action; quint32 mask; qint32 kind;
        in >> id >> action >> mask >> kind >> r.pattern >> r.hostAnchor >> r.rawPattern >> r.thirdPartyOnly >> r.firstPartyOnly >> r.isImportant >> r.includeDomains >> r.excludeDomains;
        r.id = id; r.action = (RuleAction)action; r.resourceMask = mask; r.kind = (MatchKind)kind;
        if (r.kind == MatchKind::Regex) {
            r.compiledRegex = QRegularExpression(r.pattern, QRegularExpression::CaseInsensitiveOption);
            if (!r.compiledRegex.isValid()) return nullptr;
        }
        compiled->blockRules.push_back(std::move(r));
    }
    for (quint64 i = 0; i < allowCount; ++i) {
        NetworkRule r;
        quint32 id; qint32 action; quint32 mask; qint32 kind;
        in >> id >> action >> mask >> kind >> r.pattern >> r.hostAnchor >> r.rawPattern >> r.thirdPartyOnly >> r.firstPartyOnly >> r.isImportant >> r.includeDomains >> r.excludeDomains;
        r.id = id; r.action = (RuleAction)action; r.resourceMask = mask; r.kind = (MatchKind)kind;
        if (r.kind == MatchKind::Regex) {
            r.compiledRegex = QRegularExpression(r.pattern, QRegularExpression::CaseInsensitiveOption);
            if (!r.compiledRegex.isValid()) return nullptr;
        }
        compiled->allowRules.push_back(std::move(r));
    }
    // Rebuild indexes from cached rules without re-parsing source text.
    for (size_t i = 0; i < compiled->blockRules.size(); ++i) {
        const auto& r = compiled->blockRules[i];
        if (r.isImportant) compiled->importantBlockIds.append(static_cast<int>(i));
        if (r.kind == MatchKind::Suffix) {
            compiled->blockSuffixTrie.insert(r.pattern, (int)i);
            if (!r.pattern.contains('*') && !r.pattern.contains('/'))
                compiled->exactBlockMap[r.pattern].append((int)i);
        } else if (r.kind == MatchKind::DomainPath) {
            compiled->blockSuffixTrie.insert(r.hostAnchor, static_cast<int>(i));
        } else if (r.kind == MatchKind::ExactHost) {
            compiled->exactBlockMap[r.pattern].append((int)i);
        } else if (r.kind == MatchKind::Regex) {
            compiled->blockRegexIds.append((int)i);
        }
    }
    for (size_t i = 0; i < compiled->allowRules.size(); ++i) {
        const auto& r = compiled->allowRules[i];
        if (r.kind == MatchKind::Suffix) {
            compiled->allowSuffixTrie.insert(r.pattern, (int)i);
            if (!r.pattern.contains('*') && !r.pattern.contains('/'))
                compiled->exactAllowMap[r.pattern].append((int)i);
        } else if (r.kind == MatchKind::DomainPath) {
            compiled->allowSuffixTrie.insert(r.hostAnchor, static_cast<int>(i));
        } else if (r.kind == MatchKind::ExactHost) {
            compiled->exactAllowMap[r.pattern].append((int)i);
        } else if (r.kind == MatchKind::Regex) {
            compiled->allowRegexIds.append((int)i);
        }
    }
    compiled->blockTokenIndex.build(compiled->blockRules);
    compiled->allowTokenIndex.build(compiled->allowRules);
    for (size_t i = 0; i < compiled->blockRules.size(); ++i) {
        const auto& r = compiled->blockRules[i];
        addAhoOrLinearFallback(r, static_cast<int>(i), compiled->blockAho,
                               compiled->blockLinearIds);
    }
    for (size_t i = 0; i < compiled->allowRules.size(); ++i) {
        const auto& r = compiled->allowRules[i];
        addAhoOrLinearFallback(r, static_cast<int>(i), compiled->allowAho,
                               compiled->allowLinearIds);
    }
    compiled->blockAho.build();
    compiled->allowAho.build();

    // Read cosmetic rules when present in the current cache format.
    if (in.status() == QDataStream::Ok) {
        quint64 cosmeticCount = 0;
        in >> cosmeticCount;
        if (in.status() == QDataStream::Ok && cosmeticCount <= kMaxCosmeticRules) {
            compiled->cosmeticRulesStore.reserve(cosmeticCount);
            for (quint64 i = 0; i < cosmeticCount; ++i) {
                CosmeticRule cr;
                in >> cr.domains >> cr.selector >> cr.isException >> cr.isScriptlet >> cr.raw;
                if (in.status() != QDataStream::Ok) break;
                compiled->cosmeticRulesStore.push_back(std::move(cr));
            }
            quint64 cosmeticExcCount = 0;
            in >> cosmeticExcCount;
            if (in.status() == QDataStream::Ok && cosmeticExcCount <= kMaxCosmeticRules &&
                cosmeticCount + cosmeticExcCount <= kMaxCosmeticRules) {
                compiled->cosmeticExceptionsStore.reserve(cosmeticExcCount);
                for (quint64 i = 0; i < cosmeticExcCount; ++i) {
                    CosmeticRule cr;
                    in >> cr.domains >> cr.selector >> cr.isException >> cr.isScriptlet >> cr.raw;
                    if (in.status() != QDataStream::Ok) break;
                    compiled->cosmeticExceptionsStore.push_back(std::move(cr));
                }
            }
            // Rebuild cosmetic engine
            compiled->cosmetic.clear();
            for (auto& cr : compiled->cosmeticRulesStore) compiled->cosmetic.addRule(cr);
            for (auto& ce : compiled->cosmeticExceptionsStore) compiled->cosmetic.addRule(ce);
            compiled->cosmetic.build();
        }
    }

    return compiled;
}

} // namespace virgin::adblock
