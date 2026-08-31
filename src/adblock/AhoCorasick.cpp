#include "AhoCorasick.hpp"
#include <QQueue>
#include <limits>

namespace virgin::adblock {

void AhoCorasick::addPattern(const QString& pattern, int ruleId) {
    if (pattern.isEmpty()) return;
    int v = 0;
    for (QChar c : pattern) {
        // Lowercase already done by caller
        auto it = nodes_[v].next.find(c);
        if (it == nodes_[v].next.end()) {
            if (nodes_.size() >= std::numeric_limits<int>::max()) return;
            const int nv = static_cast<int>(nodes_.size());
            nodes_[v].next.insert(c, nv);
            nodes_.append(AhoNode());
            v = nv;
        } else {
            v = it.value();
        }
    }
    nodes_[v].out.append(ruleId);
    built_ = false;
}

void AhoCorasick::build() {
    if (built_) return;
    QQueue<int> q;
    // Depth 1 links -> root
    for (auto it = nodes_[0].next.begin(); it != nodes_[0].next.end(); ++it) {
        int v = it.value();
        nodes_[v].link = 0;
        q.enqueue(v);
    }
    while (!q.isEmpty()) {
        int v = q.dequeue();
        for (auto it = nodes_[v].next.begin(); it != nodes_[v].next.end(); ++it) {
            QChar c = it.key();
            int u = it.value();
            int j = nodes_[v].link;
            while (j != 0 && !nodes_[j].next.contains(c)) j = nodes_[j].link;
            if (nodes_[j].next.contains(c)) j = nodes_[j].next.value(c);
            nodes_[u].link = j;
            // Merge output
            nodes_[u].out += nodes_[j].out;
            q.enqueue(u);
        }
        // For missing transitions, we could fill goto, but search will follow links
    }
    built_ = true;
}

std::vector<int> AhoCorasick::search(const QString& text) const {
    std::vector<int> res;
    if (nodes_.isEmpty()) return res;
    int v = 0;
    for (QChar c : text) {
        while (v != 0 && !nodes_[v].next.contains(c)) v = nodes_[v].link;
        auto it = nodes_[v].next.find(c);
        if (it != nodes_[v].next.end()) v = it.value();
        else v = 0;
        if (!nodes_[v].out.isEmpty()) {
            for (int id : nodes_[v].out) res.push_back(id);
        }
    }
    return res;
}

} // namespace virgin::adblock
