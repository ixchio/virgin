#pragma once

#include <QString>
#include <QHash>
#include <vector>

namespace virgin::adblock {
struct NetworkRule;
class RuleIndex {
public:
    void build(const std::vector<NetworkRule>& rules);
    std::vector<int> candidatesFor(const QString& url) const;
    size_t tokenCount() const { return static_cast<size_t>(tokenMap_.size()); }
    void clear() { tokenMap_.clear(); }
private:
    QHash<QString, std::vector<int>> tokenMap_;
    // For performance, keep a set of all tokens length >=4
};

} // namespace virgin::adblock
