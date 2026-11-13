#include "app.hpp"
#include "connector_uninstall.hpp"
#include "interface_text.hpp"
#include <windowsx.h>
#include <shellapi.h>
#include <chrono>
namespace {
ss::App *app = nullptr;
ss::UI *ui = nullptr;
float dpi = 1;
bool sizing = false;
BOOL (WINAPI *swapInterval)(int) = nullptr;
std::function<void()> renderFrame;
RECT animationFrom{}, animationTo{}, restoredRect{};
ULONGLONG animationStart = 0;
bool windowAnimation = false, animationMaximize = false;
void updateWindowShape(HWND h) {
    static HWND lastWindow = nullptr;
    static int lastWidth = -1, lastHeight = -1, lastDiameter = -1;
    static bool lastMaximized = false;
    RECT bounds{};
    if (!GetWindowRect(h, &bounds)) return;
    int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return;
    UINT dpiValue = GetDpiForWindow(h);
    int diameter = MulDiv(24, dpiValue ? dpiValue : 96, 96);
    bool maximized = IsZoomed(h);
    if (lastWindow == h && lastWidth == width && lastHeight == height && lastDiameter == diameter && lastMaximized == maximized) return;
    lastWindow = h; lastWidth = width; lastHeight = height; lastDiameter = diameter; lastMaximized = maximized;
    if (maximized) { SetWindowRgn(h, nullptr, TRUE); return; }
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, diameter, diameter);
    if (region && !SetWindowRgn(h, region, TRUE)) DeleteObject(region);
}
void toggleWindow(HWND h) {
    if(windowAnimation) return;
    bool maximize = !IsZoomed(h);
    if(app && app->reduced) {ShowWindow(h, maximize ? SW_MAXIMIZE : SW_RESTORE);return;}
    GetWindowRect(h, &animationFrom);
    if(maximize) {
        restoredRect=animationFrom;
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &monitor);
        animationTo=monitor.rcWork;
    } else {
        WINDOWPLACEMENT placement{sizeof(placement)};
        GetWindowPlacement(h, &placement);
        animationTo=restoredRect.right>restoredRect.left?restoredRect:placement.rcNormalPosition;
        ShowWindow(h, SW_RESTORE);
        SetWindowPos(h,nullptr,animationFrom.left,animationFrom.top,animationFrom.right-animationFrom.left,animationFrom.bottom-animationFrom.top,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    animationMaximize=maximize;windowAnimation=true;animationStart=GetTickCount64();
    SetTimer(h,2,10,nullptr);
}
} // namespace
static LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    try {
        switch (m) {
        case WM_GETOBJECT:
            if (ui && DWORD(l)==DWORD(OBJID_CLIENT)) return ui->accessibilityObject(w);
            break;
        case ss::AccessibilityCommandMessage:
            return ui ? ui->accessibilityCommand(uint64_t(w)) : 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            updateWindowShape(h);
            if (sizing && renderFrame) renderFrame();
            return 0;
        case WM_NCCALCSIZE: {
            // Windows maximizes overlapped windows with an invisible resize frame outside rcWork.
            // Keep only the painted client area inside rcWork; retain native DWM state/animations.
            if(IsZoomed(h)){
                MONITORINFO monitor{sizeof(monitor)};
                if(GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&monitor)){
                    auto *rect=w?&reinterpret_cast<NCCALCSIZE_PARAMS*>(l)->rgrc[0]:reinterpret_cast<RECT*>(l);
                    rect->left=std::max(rect->left,monitor.rcWork.left);
                    rect->top=std::max(rect->top,monitor.rcWork.top);
                    rect->right=std::min(rect->right,monitor.rcWork.right);
                    rect->bottom=std::min(rect->bottom,monitor.rcWork.bottom);
                }
            }
            return 0;
        }
        case WM_APP + 20:
            toggleWindow(h);
            return 0;
        case WM_ENTERSIZEMOVE:
            sizing = true;
            if (swapInterval) swapInterval(0);
            return 0;
        case WM_EXITSIZEMOVE:
            sizing = false;
            if (swapInterval) swapInterval(1);
            if (renderFrame) renderFrame();
            return 0;
        case WM_TIMER:
            if(w==2 && windowAnimation) {
                float t=std::min(1.f,float(GetTickCount64()-animationStart)/230.f);
                float eased=1-std::pow(1-t,3.f);
                auto lerp=[&](LONG a,LONG b){return LONG(std::lround(a+(b-a)*eased));};
                RECT r{lerp(animationFrom.left,animationTo.left),lerp(animationFrom.top,animationTo.top),lerp(animationFrom.right,animationTo.right),lerp(animationFrom.bottom,animationTo.bottom)};
                SetWindowPos(h,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE);
                if(renderFrame) renderFrame();
                if(t>=1){KillTimer(h,2);windowAnimation=false;if(animationMaximize)ShowWindow(h,SW_MAXIMIZE);}
                return 0;
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto *p = (MINMAXINFO *)l;
            MONITORINFO monitor{sizeof(monitor)};
            if (GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &monitor)) {
                auto &work = monitor.rcWork;
                auto &screen = monitor.rcMonitor;
                p->ptMaxPosition = {work.left - screen.left, work.top - screen.top};
                p->ptMaxSize = {work.right - work.left, work.bottom - work.top};
                p->ptMinTrackSize = {std::min(LONG(880 * dpi), work.right - work.left),
                                     std::min(LONG(590 * dpi), work.bottom - work.top)};
            }
            return 0;
        }
        case WM_DPICHANGED: {
            dpi = HIWORD(w) / 96.f;
            auto *r = (RECT *)l;
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            updateWindowShape(h);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            ScreenToClient(h, &p);
            RECT r;
            GetClientRect(h, &r);
            int b = LONG(8 * dpi);
            if (!IsZoomed(h)) {
                bool left = p.x < b, right = p.x >= r.right - b, top = p.y < b, bottom = p.y >= r.bottom - b;
                if (top && left)
                    return HTTOPLEFT;
                if (top && right)
                    return HTTOPRIGHT;
                if (bottom && left)
                    return HTBOTTOMLEFT;
                if (bottom && right)
                    return HTBOTTOMRIGHT;
                if (left)
                    return HTLEFT;
                if (right)
                    return HTRIGHT;
                if (top)
                    return HTTOP;
                if (bottom)
                    return HTBOTTOM;
            }
            if (ui && ui->draggingTitle(float(p.x), float(p.y)))
                return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SETCURSOR:
            if (LOWORD(l) == HTCLIENT) return TRUE;
            break;
        case WM_DROPFILES: {
            HDROP drop=reinterpret_cast<HDROP>(w);std::vector<std::string> paths;
            UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
            if(app&&ui&&ui->acceptsFileDrop()&&count<=256){
                for(UINT i=0;i<count;++i){UINT length=DragQueryFileW(drop,i,nullptr,0);if(!length||length>32767)continue;
                    std::wstring path(size_t(length)+1,L'\0');DragQueryFileW(drop,i,path.data(),length+1);path.resize(length);paths.push_back(ss::utf8(path));}
                DragFinish(drop);app->addLocalFiles(paths);
            } else {DragFinish(drop);if(app)app->notify(count>256?"Можно добавить не более 256 файлов за раз":"Перетащите файлы на вкладку «Загрузки»");}
            return 0;
        }
        case WM_MOUSEMOVE:
            if (ui)
                ui->mouse(float(GET_X_LPARAM(l)), float(GET_Y_LPARAM(l)));
            return 0;
        case WM_RBUTTONUP:
            if(ui)ui->rightClick(float(GET_X_LPARAM(l)),float(GET_Y_LPARAM(l)));
            return 0;
        case WM_LBUTTONDOWN:
            if (ui) {
                ui->mouse(float(GET_X_LPARAM(l)), float(GET_Y_LPARAM(l)));
                ui->press();
                SetCapture(h);
            }
            return 0;
        case WM_LBUTTONUP:
            if (ui) {
                ui->mouse(float(GET_X_LPARAM(l)), float(GET_Y_LPARAM(l)));
                ui->release();
                ReleaseCapture();
            }
            return 0;
        case WM_CAPTURECHANGED:
        case WM_KILLFOCUS:
            if (ui)
                ui->lostCapture();
            return 0;
        case WM_MOUSEWHEEL:
            if (ui)
                ui->wheel(GET_WHEEL_DELTA_WPARAM(w) / 120.f, GetKeyState(VK_SHIFT) < 0);
            return 0;
        case WM_CHAR:
            if (ui)
                ui->character(wchar_t(w));
            return 0;
        case WM_KEYDOWN:
            if (ui)
                ui->key(int(w), GetKeyState(VK_CONTROL) < 0, GetKeyState(VK_SHIFT) < 0);
            return 0;
        case WM_CLOSE:
            if (app)
                app->closeRequest();
            else
                DestroyWindow(h);
            return 0;
        case WM_SYSCOMMAND:
            if((w & 0xfff0)==SC_MAXIMIZE || ((w & 0xfff0)==SC_RESTORE && IsZoomed(h)&&!IsIconic(h))) {
                toggleWindow(h);
                return 0;
            }
            if ((w & 0xfff0) == SC_CLOSE) {
                if (app)
                    app->closeRequest();
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception &e) {
        if (app)
            app->error(e.what());
    }
    return DefWindowProcW(h, m, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    using namespace ss;
    std::string startupLanguage = defaultInterfaceLanguage(GetUserDefaultUILanguage());
    int smokeExit = 0;
    GuidedBrowser smokeBrowser = GuidedBrowser::None;
    fs::path smokeReport;
    bool watchInstallation = false, watchToolbar = false;
    bool unregisterIfOwned = false;
    {
        int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; argv && i < argc; ++i) {
            if (std::wstring(argv[i]) == L"--test-browser-guide" && i + 1 < argc) {
                std::wstring name = argv[++i];
                smokeBrowser = name == L"chrome" ? GuidedBrowser::Chrome : name == L"yandex" ? GuidedBrowser::Yandex : GuidedBrowser::None;
            } else if (std::wstring(argv[i]) == L"--test-report" && i + 1 < argc) smokeReport = argv[++i];
            else if (std::wstring(argv[i]) == L"--test-installation-flow") watchInstallation = true;
            else if (std::wstring(argv[i]) == L"--test-toolbar-flow") watchToolbar = true;
            else if (std::wstring(argv[i]) == L"--unregister-connector-if-owned") unregisterIfOwned = true;
        }
        if (argv) LocalFree(argv);
    }
    try {
        auto filename = executable().filename().wstring();
        if (filename == L"StreamShelf.Host.exe")
            return nativeHostMain();
        if (unregisterIfOwned) {
            try { unregisterConnectorIfOwned(dataRoot(), executable(), HKEY_CURRENT_USER); return 0; }
            catch (...) { return 1; }
        }
        const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        struct FinishCom {HRESULT result;~FinishCom(){if(SUCCEEDED(result))CoUninitialize();}} finishCom{comResult};
        try {
            const auto settings = dataRoot() / L"settings.dpapi";
            if (fs::exists(settings)) {
                const auto saved = str(Json::parse(unprotectFile(settings)), "interfaceLanguage");
                if (saved == "en" || saved == "ru") startupLanguage = saved;
            }
        } catch (...) {}
        Handle mutex(CreateMutexW(nullptr, TRUE, (L"Local\\" + pipeName().substr(9)).c_str()));
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            MessageBoxW(nullptr, startupLanguage == "ru" ? L"StreamShelf уже открыт." : L"StreamShelf is already running.", L"StreamShelf", MB_OK);
            return 0;
        }
        auto setDpi = (BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT))GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                                            "SetProcessDpiAwarenessContext");
        if (setDpi)
            setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        WNDCLASSEXW cls{sizeof(cls)};
        cls.style = CS_OWNDC;
        cls.lpfnWndProc = WindowProc;
        cls.hInstance = instance;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = L"StreamShelfNativeOpenGL";
        cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
        cls.hIconSm = cls.hIcon;
        if (!RegisterClassExW(&cls))
            throw std::runtime_error("RegisterClass");
        POINT cursor{};
        GetCursorPos(&cursor);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor);
        auto &work = monitor.rcWork;
        int workWidth = work.right - work.left, workHeight = work.bottom - work.top;
        int initialWidth = std::min(1440, workWidth * 94 / 100);
        int initialHeight = std::min(920, workHeight * 94 / 100);
        HWND h =
            CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"StreamShelf · C++ / WinAPI / OpenGL",
                            WS_OVERLAPPEDWINDOW,
                            work.left + (workWidth - initialWidth) / 2,
                            work.top + (workHeight - initialHeight) / 2,
                            initialWidth, initialHeight, nullptr, nullptr, instance, nullptr);
        if (!h)
            throw std::runtime_error("CreateWindow");
        DragAcceptFiles(h, TRUE);
        if(auto dwm=LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
            auto attribute=reinterpret_cast<HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD)>(GetProcAddress(dwm,"DwmSetWindowAttribute"));
            if(attribute){BOOL dark=TRUE;attribute(h,20,&dark,sizeof(dark));COLORREF border=0xfffffffe;attribute(h,34,&border,sizeof(border));}
            FreeLibrary(dwm);
        }
        SetWindowPos(h,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        updateWindowShape(h);
        HDC dc = GetDC(h);
        PIXELFORMATDESCRIPTOR pfd{sizeof(pfd),
                                  1,
                                  PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA,
                                  32,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  8,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  PFD_MAIN_PLANE,
                                  0,
                                  0,
                                  0,
                                  0};
        int format = ChoosePixelFormat(dc, &pfd);
        if (!format || !SetPixelFormat(dc, format, &pfd))
            throw std::runtime_error("OpenGL pixel format");
        HGLRC legacy = wglCreateContext(dc);
        if (!legacy || !wglMakeCurrent(dc, legacy))
            throw std::runtime_error("OpenGL context");
        using Create = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
        auto create = (Create)wglGetProcAddress("wglCreateContextAttribsARB");
        if (!create)
            throw std::runtime_error(
                "Требуется OpenGL 3.3. Установите драйвер видеокарты Intel / AMD / NVIDIA.");
        int attributes[] = {0x2091, 3, 0x2092, 3, 0x9126, 1, 0};
        HGLRC gl = create(dc, nullptr, attributes);
        if (!gl)
            throw std::runtime_error("Не удалось создать OpenGL 3.3 core context");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacy);
        if (!wglMakeCurrent(dc, gl))
            throw std::runtime_error("wglMakeCurrent");
        swapInterval = (BOOL(WINAPI *)(int))wglGetProcAddress("wglSwapIntervalEXT");
        if (swapInterval)
            swapInterval(1);
        {
            App model;
            app = &model;
            startupLanguage = model.interfaceLanguage;
            model.hwnd = h;
            UI screen(model);
            ui = &screen;
            screen.init();
            model.log("C++ / WinAPI · " + screen.driver());
            bool savedAutoUpdate = model.autoUpdate;
            bool smoke = smokeBrowser != GuidedBrowser::None && !smokeReport.empty();
            if (smoke) model.autoUpdate = false;
            model.start();
            bool smokeStarted = false, smokeFinished = false;
            ULONGLONG smokeBegan = GetTickCount64(), smokeReportAt = 0;
            Json smokeStages = Json::array(); int lastSmokeStage = -2;
            bool observedAbsent = false;
            ULONGLONG installationVerifiedAt = 0;
            auto getDpi =
                (UINT(WINAPI *)(HWND))GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
            if (getDpi)
                dpi = getDpi(h) / 96.f;
            ShowWindow(h, SW_MAXIMIZE);
            UpdateWindow(h);
            auto previous = std::chrono::steady_clock::now();
            auto nextFrame = previous;
            renderFrame = [&] {
                if (!screen.interactionInProgress() && model.drain()) screen.invalidateModelSnapshot();
                if (IsIconic(h))
                    return;
                RECT r;
                GetClientRect(h, &r);
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - previous).count();
                previous = now;
                screen.frame(r.right, r.bottom, dt, dpi, sizing || windowAnimation);
                SwapBuffers(dc);
            };
            bool run = true;
            while (run) {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        run = false;
                        break;
                    }
                    if (model.extensionInstaller.translate(msg)) continue;
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                if (!run)
                    break;
                if (!screen.interactionInProgress() && model.drain()) {
                    screen.invalidateModelSnapshot();
                    nextFrame = std::chrono::steady_clock::now();
                }
                if (smoke && !smokeFinished) {
                    auto now = GetTickCount64();
                    // Match the real button's precondition: StreamShelf owns foreground focus.
                    if (!smokeStarted && !model.browsers.empty() && GetForegroundWindow() == h) {
                        for (int i = 0; i < int(model.browsers.size()); ++i) {
                            if (guidedBrowser(model.browsers[i]) == smokeBrowser) {
                                model.browserIndex = i; model.profileIndex = 0;
                                model.action("prepareConnector"); smokeStarted = true; break;
                            }
                        }
                    }
                    if (now - smokeReportAt > 250) {
                        smokeReportAt = now;
                        auto state = model.extensionInstaller.diagnostics();
                        bool passed = state.value("page_verified", false) && state.value("panel_visible", false) &&
                            state.value("outline_visible", false) && (state.value("developer_found", false) || state.value("load_found", false) || state.value("enable_confirmation_dialog", false));
                        if (state.value("page_verified", false) && !state.value("active", true) &&
                            state.value("extension_present", false) && state.value("extension_enabled_known", false) &&
                            state.value("extension_enabled", false) && state.value("stage", -1) >= 6) passed = true;
                        int guideStage = state.value("stage", -1);
                        if (guideStage != lastSmokeStage) {
                            smokeStages.push_back({{"stage", guideStage}, {"elapsed_ms", now - smokeBegan},
                                {"present", state.value("extension_present", false)}, {"enabled", state.value("extension_enabled", false)}});
                            lastSmokeStage = guideStage;
                        }
                        if (state.value("page_observed", false) && !state.value("extension_present", false)) observedAbsent = true;
                        if (watchInstallation) {
                            bool installed = state.value("extension_present", false) && state.value("extension_enabled_known", false) && state.value("extension_enabled", false);
                            bool ready = guideStage >= 6 && installed;
                            if (ready && !installationVerifiedAt) installationVerifiedAt = now;
                            if (!ready) installationVerifiedAt = 0;
                            passed = observedAbsent && installationVerifiedAt && now - installationVerifiedAt >= 1500;
                        }
                        if (watchToolbar) passed = state.value("connection_confirmed", false) && !state.value("active", true);
                        state["entry_point"] = "App::action(prepareConnector)";
                        state["installation_flow"] = watchInstallation; state["initial_absence_observed"] = observedAbsent; state["stage_history"] = smokeStages;
                        state["elapsed_ms"] = now - smokeBegan; state["passed"] = passed;
                        state["preparing"] = model.preparingInstaller;
                        if (!model.modalTitle.empty()) state["application_error"] = model.modalBody;
                        writeFile(smokeReport, state.dump(2));
                        if ((passed && now - smokeBegan > 4500) || now - smokeBegan > (watchInstallation ? 360000 : watchToolbar ? 150000 : 30000)) {
                            smokeExit = passed ? 0 : 3; smokeFinished = true;
                            model.extensionInstaller.close(); model.autoUpdate = savedAutoUpdate; model.exitRequested = true;
                        }
                    }
                }
                if (IsIconic(h)) {
                    MsgWaitForMultipleObjects(0, nullptr, FALSE, 250, QS_ALLINPUT);
                    nextFrame = std::chrono::steady_clock::now();
                    continue;
                }
                // An inactive window does not need a full animation frame rate. Keep
                // processing Windows messages and background results while pacing
                // frames independently of whether the graphics driver blocks in SwapBuffers.
                auto frameTime = std::chrono::steady_clock::now();
                if (frameTime < nextFrame) {
                    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - frameTime).count();
                    MsgWaitForMultipleObjects(0, nullptr, FALSE, DWORD(std::max<int64_t>(1, remaining)), QS_ALLINPUT);
                    continue;
                }
                bool active = GetForegroundWindow() == h || sizing || windowAnimation;
                nextFrame = frameTime + std::chrono::milliseconds(active ? 16 : 125);
                RECT r;
                GetClientRect(h, &r);
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - previous).count();
                previous = now;
                screen.frame(r.right, r.bottom, dt, dpi, sizing || windowAnimation);
                SwapBuffers(dc);
                if (screen.finished()) {
                    DestroyWindow(h);
                    run = false;
                }
            }
            if (smoke) model.autoUpdate = savedAutoUpdate;
            renderFrame = nullptr;
            screen.destroy();
            ui = nullptr;
            app = nullptr;
        }
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(gl);
        ReleaseDC(h, dc);
        return smokeExit;
    } catch (const std::exception &e) {
        std::wstring s;
        try {
            s = ss::wide(ss::interfaceText(startupLanguage, ss::redact(e.what())));
        } catch (...) {
            s = startupLanguage == "ru" ? L"Ошибка запуска приложения" : L"Could not start the application";
        }
        MessageBoxW(nullptr, s.c_str(), L"StreamShelf · C++", MB_ICONERROR | MB_OK);
        return 1;
    }
}
