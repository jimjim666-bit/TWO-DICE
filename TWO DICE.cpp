#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <string>
#include <cstdlib>
#include <ctime>
using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

static HINSTANCE g_hInst;
static HWND g_hWnd;
static ULONG_PTR g_gdiplusToken;

static const int TIMER_ROLL = 1001;
static const int TIMER_BG = 1002;
static const int ROLL_TICK_MS = 60;
static const int ROLL_TICKS = 25;
static const int BG_TICK_MS = 33;
static const int MAX_PARTICLES = 25;

static int  g_die1 = 1, g_die2 = 1;
static int  g_disp1 = 1, g_disp2 = 1;
static bool g_rolling = false;
static int  g_tickCount = 0;
static int  g_point = 0;
static int  g_balance = 100;
static std::wstring g_msg = L"点击下方按钮投掷骰子";
static RECT g_btnRect = { 0,0,0,0 };
static RECT g_ruleBtnRect = { 0,0,0,0 };
static bool g_showRules = false;

// 粒子
struct Particle { float x, y, vx, vy, r; Color c; };
static Particle g_p[MAX_PARTICLES];
static float g_bgHue = 0.0f;

static void InitParticles(int W, int H)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_p[i].x = (float)(rand() % W);
        g_p[i].y = (float)(rand() % H);
        g_p[i].vx = (float)((rand() % 100 - 50) / 500.0f);
        g_p[i].vy = (float)((rand() % 100 - 50) / 500.0f);
        g_p[i].r = 2.0f + rand() % 4;
        int hue = rand() % 360;
        g_p[i].c = Color(120, 255, 200, 100);
    }
}

static void UpdateParticles(int W, int H)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_p[i].x += g_p[i].vx;
        g_p[i].y += g_p[i].vy;
        if (g_p[i].x < -10) g_p[i].x = W + 10;
        if (g_p[i].x > W + 10) g_p[i].x = -10;
        if (g_p[i].y < -10) g_p[i].y = H + 10;
        if (g_p[i].y > H + 10) g_p[i].y = -10;
    }
    g_bgHue += 0.3f;
    if (g_bgHue >= 360.0f) g_bgHue -= 360.0f;
}

// HSL 转 RGB 简化版
static Color HSL(float h, float s, float l, int alpha = 255)
{
    float c = (1 - fabsf(2 * l - 1)) * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2.0f) - 1));
    float m = l - c / 2;
    float r, g, b;
    int hi = (int)(h / 60.0f) % 6;
    if (hi == 0) { r = c; g = x; b = 0; }
    else if (hi == 1) { r = x; g = c; b = 0; }
    else if (hi == 2) { r = 0; g = c; b = x; }
    else if (hi == 3) { r = 0; g = x; b = c; }
    else if (hi == 4) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return Color(alpha, (int)((r + m) * 255), (int)((g + m) * 255), (int)((b + m) * 255));
}

void DrawDie(Graphics& g, int x, int y, int size, int value)
{
    // 投影
    SolidBrush shadow(Color(80, 0, 0, 0));
    g.FillEllipse(&shadow, (REAL)x + 3, (REAL)(y + size + 6), (REAL)(size - 6), (REAL)10);

    GraphicsPath path;
    int r = size / 4;
    path.AddArc((REAL)x, (REAL)y, (REAL)(2 * r), (REAL)(2 * r), 180, 90);
    path.AddArc((REAL)(x + size - 2 * r), (REAL)y, (REAL)(2 * r), (REAL)(2 * r), 270, 90);
    path.AddArc((REAL)(x + size - 2 * r), (REAL)(y + size - 2 * r), (REAL)(2 * r), (REAL)(2 * r), 0, 90);
    path.AddArc((REAL)x, (REAL)(y + size - 2 * r), (REAL)(2 * r), (REAL)(2 * r), 90, 90);
    path.CloseFigure();

    // 骰子主体: 白到浅蓝灰渐变
    LinearGradientBrush body(Rect(x, y, size, size),
        Color(255, 255, 255, 255), Color(255, 195, 205, 225),
        LinearGradientModeVertical);
    g.FillPath(&body, &path);

    // 顶部高光
    LinearGradientBrush hl(Rect(x + 8, y + 6, size - 16, size / 3),
        Color(180, 255, 255, 255), Color(0, 255, 255, 255),
        LinearGradientModeVertical);
    g.FillPath(&hl, &path);

    Pen border(Color(255, 70, 80, 100), 1.5f);
    g.DrawPath(&border, &path);

    const REAL L = 0.25f, R = 0.75f, C = 0.5f;
    PointF spots[6]; int count = 0;
    switch (value) {
    case 1: spots[0] = PointF(C, C); count = 1; break;
    case 2: spots[0] = PointF(L, L); spots[1] = PointF(R, R); count = 2; break;
    case 3: spots[0] = PointF(L, L); spots[1] = PointF(C, C); spots[2] = PointF(R, R); count = 3; break;
    case 4: spots[0] = PointF(L, L); spots[1] = PointF(R, L); spots[2] = PointF(L, R); spots[3] = PointF(R, R); count = 4; break;
    case 5: spots[0] = PointF(L, L); spots[1] = PointF(R, L); spots[2] = PointF(C, C); spots[3] = PointF(L, R); spots[4] = PointF(R, R); count = 5; break;
    case 6: spots[0] = PointF(L, L); spots[1] = PointF(R, L); spots[2] = PointF(L, C); spots[3] = PointF(R, C); spots[4] = PointF(L, R); spots[5] = PointF(R, R); count = 6; break;
    }
    REAL radius = size / 10.0f;
    SolidBrush pip(Color(255, 40, 45, 65));
    for (int i = 0; i < count; i++) {
        REAL cx = x + spots[i].X * size, cy = y + spots[i].Y * size;
        g.FillEllipse(&pip, cx - radius, cy - radius, radius * 2, radius * 2);
        // 点高光
        SolidBrush piphl(Color(80, 255, 255, 255));
        g.FillEllipse(&piphl, cx - radius / 3, cy - radius / 3, radius * 0.8f, radius * 0.8f);
    }
}

void StartRoll()
{
    if (g_rolling) return;
    if (g_balance <= 0) { g_msg = L"余额为0, 重启再来"; InvalidateRect(g_hWnd, NULL, FALSE); return; }
    g_rolling = true; g_tickCount = 0;
    g_msg = L"滚动中...";
    SetTimer(g_hWnd, TIMER_ROLL, ROLL_TICK_MS, NULL);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

void FinishRoll()
{
    KillTimer(g_hWnd, TIMER_ROLL);
    g_rolling = false;
    g_die1 = 1 + rand() % 6; g_die2 = 1 + rand() % 6;
    g_disp1 = g_die1; g_disp2 = g_die2;
    int sum = g_die1 + g_die2;
    std::wstring s = L"掷出 " + std::to_wstring(g_die1) + L"+" + std::to_wstring(g_die2)
        + L"=" + std::to_wstring(sum) + L"  ";
    if (g_point == 0) {
        if (sum == 7 || sum == 11) { s += L"天然胜! +10"; g_balance += 10; }
        else if (sum == 2 || sum == 3 || sum == 12) { s += L"Craps! -10"; g_balance -= 10; }
        else { g_point = sum; s += L"点数 " + std::to_wstring(g_point); }
    }
    else {
        if (sum == g_point) { s += L"命中! +10"; g_balance += 10; g_point = 0; }
        else if (sum == 7) { s += L"出7了! -10"; g_balance -= 10; g_point = 0; }
        else s += L"继续";
    }
    g_msg = s;
    InvalidateRect(g_hWnd, NULL, FALSE);
}

void DrawButton(Graphics& g, RECT rc, const std::wstring& text, bool hover, bool down)
{
    int x = rc.left, y = rc.top, w = rc.right - rc.left, h = rc.bottom - rc.top;
    Color top, bot;
    if (down) { top = Color(255, 190, 50, 50); bot = Color(255, 140, 25, 25); }
    else if (hover) { top = Color(255, 255, 100, 100); bot = Color(255, 200, 55, 55); }
    else { top = Color(255, 235, 75, 75); bot = Color(255, 185, 45, 45); }
    LinearGradientBrush br(Rect(x, y, w, h), top, bot, LinearGradientModeVertical);
    GraphicsPath p; int r = h / 2;
    p.AddArc((REAL)x, (REAL)y, (REAL)(2 * r), (REAL)(2 * r), 180, 90);
    p.AddArc((REAL)(x + w - 2 * r), (REAL)y, (REAL)(2 * r), (REAL)(2 * r), 270, 90);
    p.AddArc((REAL)(x + w - 2 * r), (REAL)(y + h - 2 * r), (REAL)(2 * r), (REAL)(2 * r), 0, 90);
    p.AddArc((REAL)x, (REAL)(y + h - 2 * r), (REAL)(2 * r), (REAL)(2 * r), 90, 90);
    p.CloseFigure();
    g.FillPath(&br, &p);
    Pen edge(Color(255, 110, 25, 25), 1.5f);
    g.DrawPath(&edge, &p);
    Font font(L"Microsoft YaHei", 17, FontStyleBold, UnitPixel);
    SolidBrush tb(Color(255, 255, 255, 255));
    StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(text.c_str(), -1, &font, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h), &sf, &tb);
}

void DrawRuleButton(Graphics& g, RECT rc, bool hover)
{
    int x = rc.left, y = rc.top, w = rc.right - rc.left, h = rc.bottom - rc.top;
    Color bg = hover ? Color(200, 255, 255, 255) : Color(120, 255, 255, 255);
    SolidBrush br(bg);
    g.FillEllipse(&br, (REAL)x, (REAL)y, (REAL)w, (REAL)h);
    Pen edge(Color(200, 255, 255, 255), 1.5f);
    g.DrawEllipse(&edge, (REAL)x, (REAL)y, (REAL)w, (REAL)h);
    Font font(L"Microsoft YaHei", 16, FontStyleBold, UnitPixel);
    SolidBrush tb(Color(255, 60, 60, 80));
    StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"?", -1, &font, RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h), &sf, &tb);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static bool btnHover = false, btnDown = false, ruleHover = false;
    switch (msg) {
    case WM_CREATE:
        srand((unsigned)time(NULL));
        InitParticles(520, 520);
        SetTimer(hWnd, TIMER_BG, BG_TICK_MS, NULL);
        return 0;
    case WM_SIZE: {
        RECT rc; GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        g_btnRect.left = w / 2 - 100; g_btnRect.right = w / 2 + 100;
        g_btnRect.top = 380; g_btnRect.bottom = 425;
        g_ruleBtnRect.left = w - 55; g_ruleBtnRect.right = w - 15;
        g_ruleBtnRect.top = 15; g_ruleBtnRect.bottom = 55;
        return 0;
    }
    case WM_TIMER:
        if (wParam == TIMER_BG) {
            RECT rc; GetClientRect(hWnd, &rc);
            UpdateParticles(rc.right - rc.left, rc.bottom - rc.top);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        else if (wParam == TIMER_ROLL) {
            g_disp1 = 1 + rand() % 6; g_disp2 = 1 + rand() % 6;
            g_tickCount++;
            InvalidateRect(hWnd, NULL, FALSE);
            if (g_tickCount >= ROLL_TICKS) FinishRoll();
        }
        return 0;
    case WM_MOUSEMOVE: {
        POINT p = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        bool overBtn = PtInRect(&g_btnRect, p);
        bool overRule = PtInRect(&g_ruleBtnRect, p);
        if (overBtn != btnHover || overRule != ruleHover) {
            btnHover = overBtn; ruleHover = overRule;
            InvalidateRect(hWnd, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(tme),TME_LEAVE,hWnd,0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        btnHover = false; btnDown = false; ruleHover = false;
        InvalidateRect(hWnd, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_btnRect, p)) { btnDown = true; SetCapture(hWnd); InvalidateRect(hWnd, NULL, FALSE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT p = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
        ReleaseCapture();
        if (PtInRect(&g_ruleBtnRect, p)) {
            g_showRules = !g_showRules;
        }
        else if (g_showRules) {
            g_showRules = false;
        }
        if (btnDown && PtInRect(&g_btnRect, p) && !g_showRules) StartRoll();
        btnDown = false;
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int W = rc.right - rc.left, H = rc.bottom - rc.top;
        HDC memdc = CreateCompatibleDC(hdc);
        HBITMAP membmp = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP oldbmp = (HBITMAP)SelectObject(memdc, membmp);
        {
            Graphics g(memdc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);

            // 动态渐变背景
            Color c1 = HSL(g_bgHue, 0.6f, 0.15f);
            Color c2 = HSL(fmodf(g_bgHue + 80, 360), 0.7f, 0.25f);
            LinearGradientBrush bg(Rect(0, 0, W, H), c1, c2, LinearGradientModeVertical);
            g.FillRectangle(&bg, 0, 0, W, H);

            // 漂浮粒子
            for (int i = 0; i < MAX_PARTICLES; i++) {
                SolidBrush pb(Color(100, 255, 220 + i * 5, 100 + i * 8));
                g.FillEllipse(&pb, g_p[i].x - g_p[i].r, g_p[i].y - g_p[i].r, g_p[i].r * 2, g_p[i].r * 2);
            }

            // 标题 (两行)
            Font titleFont(L"Microsoft YaHei", 24, FontStyleBold, UnitPixel);
            SolidBrush titleBr(Color(230, 255, 230, 150));
            StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
            g.DrawString(L"jim 大王", -1, &titleFont, RectF(0, 20, (REAL)W, 35), &sf, &titleBr);
            Font titleFont2(L"Microsoft YaHei", 16, FontStyleBold, UnitPixel);
            SolidBrush titleBr2(Color(200, 255, 210, 130));
            g.DrawString(L"的骰子游戏", -1, &titleFont2, RectF(0, 55, (REAL)W, 25), &sf, &titleBr2);

            // 骰子
            int dieSize = 110;
            int b1 = g_rolling ? (rand() % 16 - 8) : 0;
            int b2 = g_rolling ? (rand() % 16 - 8) : 0;
            int gap = 60;
            int dx1 = W / 2 - (dieSize * 2 + gap) / 2;
            int dx2 = dx1 + dieSize + gap;
            DrawDie(g, dx1, 140 + b1, dieSize, g_disp1);
            DrawDie(g, dx2, 140 + b2, dieSize, g_disp2);

            // 信息栏
            Font infoFont(L"Microsoft YaHei", 15, FontStyleRegular, UnitPixel);
            SolidBrush infoBr(Color(230, 255, 255, 255));
            std::wstring line = L"余额: " + std::to_wstring(g_balance) + L" 分";
            if (g_point > 0) line += L"    点数: " + std::to_wstring(g_point);
            g.DrawString(line.c_str(), -1, &infoFont, RectF(0, 290, (REAL)W, 25), &sf, &infoBr);

            // 消息
            Font msgFont(L"Microsoft YaHei", 13, FontStyleRegular, UnitPixel);
            SolidBrush msgBr(g_rolling ? Color(200, 255, 255, 200) : Color(220, 255, 240, 180));
            g.DrawString(g_msg.c_str(), -1, &msgFont, RectF(20, 325, (REAL)(W - 40), 25), &sf, &msgBr);

            // 按钮
            DrawButton(g, g_btnRect, g_rolling ? L"ROLLING..." : L"THROW DICE", btnHover, btnDown);

            // 右上角规则按钮
            DrawRuleButton(g, g_ruleBtnRect, ruleHover);

            // 规则面板覆盖层
            if (g_showRules) {
                SolidBrush dim(Color(180, 0, 0, 0));
                g.FillRectangle(&dim, 0, 0, W, H);

                int pw = 380, ph = 320;
                int px = (W - pw) / 2, py = (H - ph) / 2;
                LinearGradientBrush panel(Rect(px, py, pw, ph),
                    Color(255, 45, 50, 75), Color(255, 30, 35, 55),
                    LinearGradientModeVertical);
                GraphicsPath pp;
                int pr = 16;
                pp.AddArc(px, py, 2 * pr, 2 * pr, 180, 90);
                pp.AddArc(px + pw - 2 * pr, py, 2 * pr, 2 * pr, 270, 90);
                pp.AddArc(px + pw - 2 * pr, py + ph - 2 * pr, 2 * pr, 2 * pr, 0, 90);
                pp.AddArc(px, py + ph - 2 * pr, 2 * pr, 2 * pr, 90, 90);
                pp.CloseFigure();
                g.FillPath(&panel, &pp);
                Pen pedge(Color(255, 255, 200, 100), 2);
                g.DrawPath(&pedge, &pp);

                Font rf(L"Microsoft YaHei", 16, FontStyleBold, UnitPixel);
                SolidBrush rt(Color(255, 255, 220, 120));
                StringFormat rcf; rcf.SetAlignment(StringAlignmentCenter);
                g.DrawString(L"游戏规则", -1, &rf, RectF(px, py + 15, pw, 30), &rcf, &rt);

                Font rb(L"Microsoft YaHei", 12, FontStyleRegular, UnitPixel);
                SolidBrush rbColor(Color(255, 230, 235, 245));
                StringFormat rlf; rlf.SetAlignment(StringAlignmentNear);
                RectF textR(px + 25, py + 60, pw - 50, ph - 80);
                g.DrawString(
                    L"首掷:\n"
                    L"  7 或 11  →  胜 (+10分)\n"
                    L"  2, 3, 12  →  负 (-10分)\n"
                    L"  其他点数  →  建立该点数\n\n"
                    L"追点阶段:\n"
                    L"  掷出该点数  →  胜 (+10分)\n"
                    L"  掷出 7      →  负 (-10分)\n"
                    L"  其他        →  继续投掷\n\n"
                    L"点任意处关闭",
                    -1, &rb, textR, &rlf, &rbColor);
            }
        }
        BitBlt(hdc, 0, 0, W, H, memdc, 0, 0, SRCCOPY);
        SelectObject(memdc, oldbmp);
        DeleteObject(membmp);
        DeleteDC(memdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        if (g_rolling) KillTimer(hWnd, TIMER_ROLL);
        KillTimer(hWnd, TIMER_BG);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    g_hInst = hInstance;
    GdiplusStartupInput gi;
    GdiplusStartup(&g_gdiplusToken, &gi, NULL);
    WNDCLASSEX wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"DiceGameClass";
    RegisterClassEx(&wc);
    int W = 520, H = 520;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - W) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    g_hWnd = CreateWindowEx(0, wc.lpszClassName, L"jim大王的骰子游戏",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        sx, sy, W, H, NULL, NULL, hInstance, NULL);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    GdiplusShutdown(g_gdiplusToken);
    return (int)msg.wParam;
}
