#include "RuleIndex.hpp"
#include "AdBlockEngine.hpp"

namespace virgin::adblock {

void RuleIndex::build(const std::vector<NetworkRule>& rules) {
    tokenMap_.clear();
    tokenMap_.reserve(static_cast<qsizetype>(rules.size() * 2U));
    for (size_t i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];
        // Only index substring-type rules that have a selective token (4+ alphanumeric)
        // Suffix and exact host are handled via trie/hash, so skip them for token index
        if (r.kind == MatchKind::Suffix || r.kind == MatchKind::DomainPath ||
            r.kind == MatchKind::ExactHost) continue;
        QString pat = r.pattern.toLower();
        // Extract longest alphanumeric tokens >=4
        QString token;
        QString best;
        for (QChar c : pat) {
            if (c.isLetterOrNumber()) {
                token += c;
            } else {
                if (token.size() >= 4) {
                    if (token.size() > best.size()) best = token;
                }
                token.clear();
            }
        }
        if (token.size() >= 4 && token.size() > best.size()) best = token;
        if (!best.isEmpty()) {
            tokenMap_[best].push_back((int)i);
        } else {
            // No selective token — fallback to generic bucket "*"
            tokenMap_["*"].push_back((int)i);
        }
    }
}

std::vector<int> RuleIndex::candidatesFor(const QString& url) const {
    QString lower = url.toLower();
    std::vector<int> out;
    out.reserve(16);

    // Tokenize URL into alphanumeric tokens >=4 and lookup
    QString token;
    for (QChar c : lower) {
        if (c.isLetterOrNumber()) {
            token += c;
        } else {
            if (token.size() >= 4) {
                auto it = tokenMap_.find(token);
                if (it != tokenMap_.end()) {
                    out.insert(out.end(), it.value().begin(), it.value().end());
                }
                // Exact token matches keep candidate sets predictable.
                // Check generic bucket for patterns without token?
            }
            token.clear();
        }
    }
    if (token.size() >= 4) {
        auto it = tokenMap_.find(token);
        if (it != tokenMap_.end()) out.insert(out.end(), it.value().begin(), it.value().end());
    }

    // Always include generic bucket "*" (rules without selective token) — they must be checked via Aho or linear
    auto itGen = tokenMap_.find("*");
    if (itGen != tokenMap_.end()) {
        out.insert(out.end(), itGen.value().begin(), itGen.value().end());
    }

    // Deduplicate via sort+unique if needed (small)
    if (out.size() > 1) {
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }
    return out;
}

} // namespace virgin::adblock
