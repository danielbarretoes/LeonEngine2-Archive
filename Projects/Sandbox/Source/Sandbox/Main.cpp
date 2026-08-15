#include "core/UEngine.hpp"
#include "OpenGLRenderDriver.hpp"

int main(int argc, char** argv) {
    Leon::FOpenGLRenderDriver::Register();
    Leon::FApplicationCommandLineArgs args{argc, argv};
    return Leon::UEngine::Run(args, "Projects/Sandbox/Sandbox.lproject");
}
