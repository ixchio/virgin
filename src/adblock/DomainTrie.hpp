#pragma once

#include <QString>
#include <unordered_map>
#include <memory>
#include <vector>

namespace virgin::adblock {

struct TrieNode {
    bool terminal = false;
    std::unordered_map<QChar, std::unique_ptr<TrieNode>> children;
    std::vector<int> ruleIds;
};

class DomainTrie {
public:
    void insert(const QString& domain, int ruleId);
    bool matches(const QString& host) const; // quick bool
    std::vector<int> findMatches(const QString& host) const; // returns all ruleIds whose domain suffix-matches host with boundary
    void clear();
    size_t size() const;
private:
    std::unique_ptr<TrieNode> root_ = std::make_unique<TrieNode>();
    size_t count_ = 0;
};

} // namespace virgin::adblock
