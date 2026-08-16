#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include "Assets/UAssetManager.hpp"
#include "RHI/FRenderer.hpp"
#include <cstdlib>

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();
    Leon::UAssetManager::Shutdown();
    Leon::FRenderer::Shutdown();
    if (context.shouldExit())
        return res;
    std::quick_exit(res);
}
