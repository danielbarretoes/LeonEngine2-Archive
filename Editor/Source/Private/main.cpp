#include "Editor/FEditorApp.hpp"

#include "FOpenGLRenderDriver.hpp"
#include "FJoltPhysicsDriver.hpp" // link Leon::Jolt — auto-registers IPhysicsScene factory

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        Leon::FOpenGLRenderDriver::Register();
        Leon::FJoltPhysicsDriver::Register();

        Leon::FApplicationCommandLineArgs Args{argc, argv};
        Leon::Editor::FEditorApp App(Args);
        App.Run();
        return 0;
    } catch (const std::exception& Ex) {
        std::cerr << "Fatal: " << Ex.what() << '\n';
        return 1;
    }
}
