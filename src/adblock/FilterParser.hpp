#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <optional>
#include "AdBlockEngine.hpp"
#include "CosmeticEngine.hpp"

namespace virgin::adblock {

class FilterParser {
public:
    struct ParseResult {
        std::vector<NetworkRule> blockRules;
        std::vector<NetworkRule> allowRules;
        std::vector<CosmeticRule> cosmeticRules;
        std::vector<CosmeticRule> cosmeticExceptions;
        int ignored = 0;
        int errors = 0;
        int cosmeticSkipped = 0;
    };

    static ParseResult parseLine(const QString& line);
    static ParseResult parseList(const QString& raw);
    static bool isComment(const QString& line);
    static bool isCosmetic(const QString& line);
    static std::optional<NetworkRule> parseNetworkRule(const QString& line, uint32_t id);
    static std::optional<CosmeticRule> parseCosmeticRule(const QString& line);

    static uint32_t parseResourceMask(const QString& options);
    static void parseDomainOption(const QString& value, NetworkRule& rule);
    static QString normalizePattern(QString pat, MatchKind& kind, QString* hostAnchor = nullptr);
};

} // namespace virgin::adblock
