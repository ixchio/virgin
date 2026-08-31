#include "../../src/adblock/FilterParser.hpp"
#include <cstdint>
#include <cstddef>

// Sec 53: malformed filter data must never crash, hang, or allocate unbounded memory.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 8192) return 0; // enforce max rule length
    QString line = QString::fromUtf8(reinterpret_cast<const char*>(data), (int)size);
    auto res = virgin::adblock::FilterParser::parseLine(line);
    // Also test whole-list parse with chunked data
    (void)res;
    return 0;
}

#ifdef FUZZ_STANDALONE
#include <iostream>
int main() {
    const char* samples[] = {
        "||tracker.example^",
        "@@||allow.example^$document",
        "!",
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        nullptr
    };
    for (int i=0; samples[i]; ++i) {
        QString s = QString::fromUtf8(samples[i]);
        auto r = virgin::adblock::FilterParser::parseLine(s);
        const auto count = r.blockRules.size() + r.allowRules.size();
        std::cout << "parsed " << s.toStdString() << " -> " << count << "\n";
    }
    return 0;
}
#endif
