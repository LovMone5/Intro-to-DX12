#include <Windows.h>
#include "BillboardsApp.h"


int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ PSTR lpCmdLine,
    _In_ int nCmdShow) {
    try {
        BillboardsApp app(hInstance);
        if (!app.Initialize())
            return 0;

        return app.Run();
    }
    catch (DxException& e) {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }

    return 0;
}
