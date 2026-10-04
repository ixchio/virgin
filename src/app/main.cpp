#include "VirginApp.hpp"
#include "app/Version.hpp"
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtGlobal>
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--runtime-version") == 0 ||
            std::strcmp(argv[index], "--version") == 0) {
            std::printf("Virgin %s\nQt %s\nChromium %s\n", virgin::app::kVersion,
                        qVersion(), qWebEngineChromiumVersion());
            return 0;
        }
    }
    virgin::app::VirginApp app(argc, argv);
    if (!app.initialize()) {
        // initialize already showed error if needed; ensure fatal dialog if headless
        return 1;
    }
    int rc = app.run();
    app.shutdown();
    return rc;
}
