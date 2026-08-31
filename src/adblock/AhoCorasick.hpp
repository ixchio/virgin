#pragma once

// Aho-Corasick batch substring matcher.
// Finds all patterns that are substrings of URL in O(n + m) instead of per-rule scan.
#include <QString>
#include <QVector>
#include <QHash>
#include <vector>

namespace virgin::adblock {

struct AhoNode {
    QHash<QChar, int> next;
    int link = 0;
    QVector<int> out; // rule indices that end here
};

class AhoCorasick {
public:
    AhoCorasick() { nodes_.append(AhoNode()); } // root
    void clear() { nodes_.clear(); nodes_.append(AhoNode()); built_ = false; }
    void addPattern(const QString& pattern, int ruleId);
    void build(); // build failure links BFS
    // Return matching rule ids for text (urlLower). No allocation on hot path beyond output vector.
    std::vector<int> search(const QString& text) const;
    bool empty() const { return nodes_.size() == 1 && nodes_[0].out.isEmpty() && nodes_[0].next.isEmpty(); }
    size_t nodeCount() const { return static_cast<size_t>(nodes_.size()); }
private:
    QVector<AhoNode> nodes_;
    bool built_ = false;
};

} // namespace virgin::adblock
