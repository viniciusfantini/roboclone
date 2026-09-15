#include "janela_teste.h"
#include "atalho.h"
#include <cstdio>

namespace {

constexpr int ID_BTN_COMPRA = 1;
constexpr int ID_BTN_VENDA = 2;
constexpr int ID_BTN_ZERAR = 3;

HWND g_destino = nullptr;

LRESULT CALLBACK WndProcTeste(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_COMPRA) {
                std::printf("[teste] botao COMPRA clicado\n");
                enviarAltTeclaComEspacamento(g_destino, 'C');
            } else if (id == ID_BTN_VENDA) {
                std::printf("[teste] botao VENDA clicado\n");
                enviarAltTeclaComEspacamento(g_destino, 'V');
            } else if (id == ID_BTN_ZERAR) {
                std::printf("[teste] botao ZERAR clicado\n");
                enviarAltTeclaComEspacamento(g_destino, 'A');
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

} // namespace

void rodarJanelaTeste(HWND destino) {
    g_destino = destino;

    HINSTANCE hInst = GetModuleHandleA(nullptr);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProcTeste;
    wc.hInstance = hInst;
    wc.lpszClassName = "RobocloneJanelaTeste";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    RegisterClassA(&wc);

    // NAO topmost (achado ao vivo, 15/09/2026: com "always on top" essa
    // janela pode cair em cima do pedaco de tela calibrado da ORIGEM e
    // travar a leitura pra sempre, ja' que nada consegue tampa-la de
    // volta). Posicao fixa no canto inferior direito da tela principal
    // (nao CW_USEDEFAULT) pra reduzir a chance de nascer em cima de
    // alguma janela do Profit -- se ainda assim sobrepuser, sem topmost
    // basta clicar na janela do Profit pra trazer ela de volta pra cima.
    int largura = 240, altura = 190;
    int x = GetSystemMetrics(SM_CXSCREEN) - largura - 20;
    int y = GetSystemMetrics(SM_CYSCREEN) - altura - 60;

    HWND janela = CreateWindowExA(
        0, "RobocloneJanelaTeste", "roboclone -- teste de atalho (destino atual)",
        (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME) | WS_VISIBLE,
        x, y, largura, altura,
        nullptr, nullptr, hInst, nullptr);

    if (!janela) {
        std::fprintf(stderr, "[teste] falha ao criar a janela de teste\n");
        return;
    }

    CreateWindowA("BUTTON", "Compra (ALT+C)", WS_CHILD | WS_VISIBLE,
                  20, 20, 180, 32, janela, (HMENU)(INT_PTR)ID_BTN_COMPRA, hInst, nullptr);
    CreateWindowA("BUTTON", "Venda (ALT+V)", WS_CHILD | WS_VISIBLE,
                  20, 62, 180, 32, janela, (HMENU)(INT_PTR)ID_BTN_VENDA, hInst, nullptr);
    CreateWindowA("BUTTON", "Zerar (ALT+A)", WS_CHILD | WS_VISIBLE,
                  20, 104, 180, 32, janela, (HMENU)(INT_PTR)ID_BTN_ZERAR, hInst, nullptr);

    std::printf("\n[teste] janela de teste aberta -- os 3 botoes mandam o atalho pra janela de\n"
                "destino escolhida, com o mesmo espacamento configurado. Feche essa janela\n"
                "(ou CTRL+C aqui no console) pra sair do \"rodar\".\n");

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
