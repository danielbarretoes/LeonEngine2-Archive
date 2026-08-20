#include "Editor/FEditorApp.hpp"

#include "FOpenGLRenderDriver.hpp"
#include "FJoltPhysicsDriver.hpp" // link Leon::Jolt — auto-registers IPhysicsScene factory

#include <exception>
#include <iostream>

int main() {
    try {
        Leon::FOpenGLRenderDriver::Register();

        Leon::Editor::FEditorApp App;
        App.Run();
        return 0;
    } catch (const std::exception& Ex) {
        std::cerr << "Fatal: " << Ex.what() << '\n';
        return 1;
    }
}
