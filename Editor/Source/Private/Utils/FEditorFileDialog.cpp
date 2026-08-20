#include "Editor/Utils/FEditorFileDialog.hpp"

#ifdef _WIN32
// clang-format off
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
// clang-format on
#endif

namespace Leon::Editor {

    namespace {

#ifdef _WIN32
        std::string WideToUtf8(const wchar_t* InWide) {
            if (!InWide || InWide[0] == L'\0')
                return {};
            int bytes = WideCharToMultiByte(CP_UTF8, 0, InWide, -1, nullptr, 0, nullptr, nullptr);
            if (bytes <= 1)
                return {};
            std::string out(static_cast<size_t>(bytes - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, InWide, -1, out.data(), bytes, nullptr, nullptr);
            return out;
        }

        std::wstring Utf8ToWide(const char* InUtf8) {
            if (!InUtf8 || InUtf8[0] == '\0')
                return {};
            int chars = MultiByteToWideChar(CP_UTF8, 0, InUtf8, -1, nullptr, 0);
            if (chars <= 1)
                return {};
            std::wstring out(static_cast<size_t>(chars - 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, InUtf8, -1, out.data(), chars);
            return out;
        }
#endif

    } // namespace

    std::string FEditorFileDialog::OpenFile(const char* InFilter, const char* InTitle) {
#ifdef _WIN32
        char file[MAX_PATH] = {};
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = InFilter ? InFilter : "Leon Project (*.lproject)\0*.lproject\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = InTitle ? InTitle : "Open Project";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn) == TRUE) {
            return std::string(file);
        }
        return {};
#else
        (void)InFilter;
        (void)InTitle;
        return {};
#endif
    }

    std::string FEditorFileDialog::SaveFile(const char* InFilter, const char* InTitle, const char* InDefaultName) {
#ifdef _WIN32
        char file[MAX_PATH] = {};
        if (InDefaultName) {
            strncpy_s(file, InDefaultName, _TRUNCATE);
        }
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = InFilter ? InFilter : "Leon Level (*.lmap)\0*.lmap\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = InTitle ? InTitle : "Save Map";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = "lmap";
        if (GetSaveFileNameA(&ofn) == TRUE) {
            return std::string(file);
        }
        return {};
#else
        (void)InFilter;
        (void)InTitle;
        (void)InDefaultName;
        return {};
#endif
    }

    std::string FEditorFileDialog::PickFolder(const char* InTitle) {
#ifdef _WIN32
        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool bNeedUninit = SUCCEEDED(hrInit) || hrInit == S_FALSE;

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog) {
            if (bNeedUninit)
                CoUninitialize();
            return {};
        }

        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options))) {
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        if (InTitle && InTitle[0] != '\0') {
            std::wstring wideTitle = Utf8ToWide(InTitle);
            if (!wideTitle.empty()) {
                dialog->SetTitle(wideTitle.c_str());
            }
        }

        std::string result;
        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    result = WideToUtf8(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
        if (bNeedUninit)
            CoUninitialize();
        return result;
#else
        (void)InTitle;
        return {};
#endif
    }

} // namespace Leon::Editor
