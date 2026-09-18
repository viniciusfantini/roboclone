// janela_teste.h -- janelinha com 3 botoes (Compra/Venda/Zerar) que manda
// o atalho equivalente pra uma janela de destino quando clicado. Usada
// durante o "rodar" pra testar manualmente, a qualquer momento, se o
// atalho ainda esta chegando na janela certa -- sem precisar parar a
// leitura nem trocar pro modo "testaratalho" separado.
#pragma once

#include <windows.h>

// cria a janela e roda a bomba de mensagens Win32 NESTA thread ate a
// janela ser fechada (bloqueia). Chamar numa thread dedicada (ou na
// thread principal, rodando a leitura em outra) -- ver main.cpp.
void rodarJanelaTeste(HWND destino);
