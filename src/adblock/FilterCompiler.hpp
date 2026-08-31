#pragma once

#include "AdBlockEngine.hpp"
#include <QString>
#include <memory>

namespace virgin::adblock {

class FilterCompiler {
public:
    static std::shared_ptr<CompiledRules> compile(const QString& rawLists);
    static std::shared_ptr<CompiledRules> compileFiles(const QStringList& paths);
    static bool writeCache(const CompiledRules& rules, const QString& path);
    static std::shared_ptr<CompiledRules> readCache(const QString& path);
};

} // namespace virgin::adblock
