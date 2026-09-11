// escrever_texto.h -- modo DEBUG: em vez de mandar atalho ALT+ (ordem de
// verdade), escreve o rotulo lido (C/V/CC/VV/Z) numa janela de texto (ex.:
// bloco de notas), pra conferir se a leitura/classificacao esta' certa
// antes de confiar no envio de atalho de verdade.
//
// Usa UI Automation (nao PostMessage de tecla) porque a caixa de texto do
// bloco de notas (ao contrario da grade do Profit) e' um controle padrao
// que expoe o Value/Text pattern -- da' pra ACRESCENTAR texto sem roubar
// foco e sem simular tecla nenhuma.
#pragma once

#include <windows.h>
#include <string>

// acrescenta uma linha ("texto\r\n") ao conteudo atual do primeiro campo
// de texto encontrado dentro da janela indicada. Devolve false se nao
// achar um campo compativel (ex.: a janela escolhida nao e' um editor de
// texto) ou se a chamada COM falhar.
bool escreverLinha(HWND janela, const std::string& texto);
