#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "core/Application.h"

class EditorApp : public FaluEngine::Application {
public:
    EditorApp() : Application({ .title = L"FaluEngine Editor", .width = 1600, .height = 900 }) {}
    void onInit()                  override {}
    void onUpdate(float deltaTime) override { (void)deltaTime; }
    void onRender()                override {}
    void onShutdown()              override {}
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    EditorApp editor;
    return editor.run();
}
