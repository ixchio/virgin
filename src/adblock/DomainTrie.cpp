#include "DomainTrie.hpp"

namespace virgin::adblock {

void DomainTrie::insert(const QString& domain, int ruleId) {
    QString lower = domain.toLower();
    QString rev;
    rev.reserve(lower.size());
    for (qsizetype i = lower.size(); i > 0; --i) rev.append(lower[i - 1]);
    TrieNode* cur = root_.get();
    for (QChar c : rev) {
        auto& child = cur->children[c];
        if (!child) child = std::make_unique<TrieNode>();
        cur = child.get();
    }
    cur->terminal = true;
    cur->ruleIds.push_back(ruleId);
    count_++;
}

bool DomainTrie::matches(const QString& host) const {
    return !findMatches(host).empty();
}

std::vector<int> DomainTrie::findMatches(const QString& host) const {
    std::vector<int> out;
    QString lower = host.toLower();
    if (lower.isEmpty()) return out;
    QString rev;
    rev.reserve(lower.size());
    for (qsizetype i = lower.size(); i > 0; --i) rev.append(lower[i - 1]);

    TrieNode* cur = root_.get();
    for (qsizetype i = 0; i < rev.size(); ++i) {
        QChar c = rev[i];
        auto it = cur->children.find(c);
        if (it == cur->children.end()) break;
        cur = it->second.get();
        if (cur->terminal) {
            // Check boundary: if we are at terminal, the next char in rev (if any) must be '.' (which is host's preceding char)
            // e.g., host "sub.tracker.example" rev "...elpmaxe.rekcart.bus"
            // After matching "example" (7 chars) we are at terminal for "example", next char is '.' -> valid
            // After matching "tracker.example" (15 chars), next char is '.' or end -> valid
            // For "nottracker.example" rev "elpmaxe.rekcartton" after matching "tracker.example" (15 chars), next char is 't' not '.' -> invalid, so don't add
            bool boundary = (i + 1 == rev.size()) || (rev[i + 1] == '.');
            if (boundary) {
                out.insert(out.end(), cur->ruleIds.begin(), cur->ruleIds.end());
            }
        }
    }
    return out;
}

void DomainTrie::clear() {
    root_ = std::make_unique<TrieNode>();
    count_ = 0;
}

size_t DomainTrie::size() const { return count_; }

} // namespace virgin::adblock
