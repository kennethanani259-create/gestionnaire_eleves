/// Point d'entree des tests. Reduit le bruit des logs applicatifs.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "core/Logger.hpp"

int main(int argc, char** argv) {
    app::Logger::instance().setLevel(app::LogLevel::Error);
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
