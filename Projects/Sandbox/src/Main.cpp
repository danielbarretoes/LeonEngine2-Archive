#include "LeonEngine.hpp"
#include "OpenGLRenderDriver.hpp"

int main(int argc, char** argv) {
    Leon::FLog::Init();
    Leon::FOpenGLRenderDriver::Register();

    return Leon::UEngine::Run({argc, argv}, "Projects/Sandbox/Config/DefaultEngine.ini");
}
