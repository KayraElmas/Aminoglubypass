// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "framework.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include <Windows.h>
#include <gl/GL.h>
#include <MinHook.h>
#pragma comment(lib, "OpenGL32.lib")

// Global değişkenler
HMODULE g_hModule;
bool g_bShowMenu = true;
HWND g_hWindow = NULL;
WNDPROC g_wndProcOriginal = NULL;

// ImGui için Win32 mesaj işleyici
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// OpenGL hook fonksiyonları ve orijinal fonksiyon işaretçileri
typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);
wglSwapBuffers_t g_OriginalwglSwapBuffers = NULL;

// WndProc hook fonksiyonu
LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Insert tuşuna basıldığında menüyü aç/kapat
    if (msg == WM_KEYDOWN && wParam == VK_INSERT)
    {
        g_bShowMenu = !g_bShowMenu;
        return TRUE;
    }

    // ImGui'ye mesajı ilet
    if (g_bShowMenu && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;

    // Orijinal window procedure fonksiyonunu çağır
    return CallWindowProc(g_wndProcOriginal, hWnd, msg, wParam, lParam);
}

// wglSwapBuffers hook fonksiyonu
BOOL WINAPI HookedwglSwapBuffers(HDC hdc)
{
    // Statik değişkenler
    static bool imguiInitialized = false;

    // ImGui henüz başlatılmadıysa başlat
    if (!imguiInitialized)
    {
        // Debug için log oluştur
        FILE* logFile = nullptr;
        fopen_s(&logFile, "imgui_init_log.txt", "w");
        if (logFile)
        {
            fprintf(logFile, "ImGui başlatılıyor...\n");
            fclose(logFile);
        }

        g_hWindow = WindowFromDC(hdc);
        if (g_hWindow)
        {
            // Pencere prosedürünü hook et
            g_wndProcOriginal = (WNDPROC)SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);

            // ImGui yükle
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui::StyleColorsDark();

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            ImGui_ImplWin32_Init(g_hWindow);
            ImGui_ImplOpenGL3_Init("#version 130");

            imguiInitialized = true;

            // Log dosyasına başarı mesajı yaz
            fopen_s(&logFile, "imgui_init_success.txt", "w");
            if (logFile)
            {
                fprintf(logFile, "ImGui başarıyla başlatıldı!\n");
                fprintf(logFile, "Pencere handle: %p\n", g_hWindow);
                fclose(logFile);
            }
        }
    }

    // Eğer ImGui başlatıldıysa ve menü gösterilmesi gerekiyorsa
    if (imguiInitialized && g_bShowMenu)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Menü GUI'sini oluştur
        ImGui::Begin("ImGui Demo", &g_bShowMenu);
        ImGui::Text("Merhaba, ImGui!");

        static float f = 0.0f;
        ImGui::SliderFloat("Float", &f, 0.0f, 1.0f);

        static float color[3] = { 0.8f, 0.2f, 0.2f };
        ImGui::ColorEdit3("Renk", color);

        if (ImGui::Button("Kapat"))
            g_bShowMenu = false;

        ImGui::End();

        // Render et
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // Orijinal fonksiyonu çağır
    return g_OriginalwglSwapBuffers(hdc);
}

// Ana thread fonksiyonu
DWORD WINAPI MainThread(LPVOID lpParam)
{
    // Debug için log dosyası oluştur
    FILE* logFile = nullptr;
    fopen_s(&logFile, "imgui_hook_log.txt", "w");
    if (logFile)
    {
        fprintf(logFile, "DLL başarıyla inject edildi\n");

        // MinHook başlat
        MH_STATUS status = MH_Initialize();
        fprintf(logFile, "MinHook başlatma sonucu: %d\n", status);

        if (status != MH_OK)
        {
            fprintf(logFile, "MinHook başlatılamadı! Hata kodu: %d\n", status);
            fclose(logFile);
            MessageBoxA(NULL, "MinHook başlatılamadı!", "Hata", MB_OK | MB_ICONERROR);
            return 1;
        }

        // OpenGL32.dll modülünü al
        HMODULE hOpenGL = GetModuleHandleA("opengl32.dll");
        fprintf(logFile, "OpenGL32.dll handle: %p\n", hOpenGL);

        if (!hOpenGL)
        {
            // GetModuleHandle başarısız olursa, yükle
            hOpenGL = LoadLibraryA("opengl32.dll");
            fprintf(logFile, "LoadLibrary OpenGL32.dll: %p\n", hOpenGL);
        }

        if (!hOpenGL)
        {
            fprintf(logFile, "OpenGL32.dll bulunamadı! GetLastError: %d\n", GetLastError());
            fclose(logFile);
            MessageBoxA(NULL, "OpenGL32.dll bulunamadı!", "Hata", MB_OK | MB_ICONERROR);
            return 1;
        }

        // wglSwapBuffers fonksiyonunun adresini al
        g_OriginalwglSwapBuffers = (wglSwapBuffers_t)GetProcAddress(hOpenGL, "wglSwapBuffers");
        fprintf(logFile, "wglSwapBuffers adresi: %p\n", g_OriginalwglSwapBuffers);

        if (!g_OriginalwglSwapBuffers)
        {
            fprintf(logFile, "wglSwapBuffers fonksiyonu bulunamadı! GetLastError: %d\n", GetLastError());
            fclose(logFile);
            MessageBoxA(NULL, "wglSwapBuffers fonksiyonu bulunamadı!", "Hata", MB_OK | MB_ICONERROR);
            return 1;
        }

        // wglSwapBuffers fonksiyonunu hook et
        status = MH_CreateHook((LPVOID)g_OriginalwglSwapBuffers, (LPVOID)HookedwglSwapBuffers, (LPVOID*)&g_OriginalwglSwapBuffers);
        fprintf(logFile, "MH_CreateHook sonucu: %d\n", status);

        if (status != MH_OK)
        {
            fprintf(logFile, "wglSwapBuffers hook oluşturulamadı! Hata kodu: %d\n", status);
            fclose(logFile);
            MessageBoxA(NULL, "wglSwapBuffers hook başarısız!", "Hata", MB_OK | MB_ICONERROR);
            return 1;
        }

        // Hook'u etkinleştir
        status = MH_EnableHook((LPVOID)g_OriginalwglSwapBuffers);
        fprintf(logFile, "MH_EnableHook sonucu: %d\n", status);

        if (status != MH_OK)
        {
            fprintf(logFile, "Hook etkinleştirilemedi! Hata kodu: %d\n", status);
            fclose(logFile);
            MessageBoxA(NULL, "Hook etkinleştirilemedi!", "Hata", MB_OK | MB_ICONERROR);
            return 1;
        }

        fprintf(logFile, "Hook başarıyla kuruldu ve etkinleştirildi!\n");
        fclose(logFile);
    }
    else
    {
        MessageBoxA(NULL, "Log dosyası oluşturulamadı!", "Uyarı", MB_OK | MB_ICONWARNING);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        // Hook'u kaldır ve MinHook'u temizle
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        // ImGui temizle
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        // Window procedure'ü eski haline getir
        if (g_hWindow && g_wndProcOriginal)
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, (LONG_PTR)g_wndProcOriginal);
        break;
    }
    return TRUE;
}