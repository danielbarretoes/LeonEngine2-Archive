#include "Editor/Context/FEditorContext.hpp"

namespace Leon::Editor {

    FEditorContext::FEditorContext() : ActiveWorld(nullptr) {}

    void FEditorContext::SetActiveWorld(UWorld* InWorld) {
        if (ActiveWorld != InWorld) {
            ActiveWorld = InWorld;
            Selection.ClearActorSelection();

            for (const auto& cb : WorldChangedCallbacks) {
                if (cb)
                    cb(ActiveWorld);
            }
        }
    }

    void FEditorContext::SetActiveProjectPath(const std::string& InPath) {
        if (ActiveProjectPath != InPath) {
            ActiveProjectPath = InPath;
            for (const auto& cb : ProjectChangedCallbacks) {
                if (cb)
                    cb(ActiveProjectPath);
            }
        }
    }

    void FEditorContext::SetActiveMapPath(const std::string& InPath) {
        ActiveMapPath = InPath;
    }

    void FEditorContext::SetStatusMessage(const std::string& InMessage) {
        StatusMessage = InMessage;
    }

    void FEditorContext::RegisterWorldChangedCallback(FWorldChangedCallback InCallback) {
        WorldChangedCallbacks.push_back(std::move(InCallback));
    }

    void FEditorContext::RegisterProjectChangedCallback(FProjectChangedCallback InCallback) {
        ProjectChangedCallbacks.push_back(std::move(InCallback));
    }

} // namespace Leon::Editor
