// calibracao.h -- fluxo interativo (console) que descobre, por clique do
// operador, a regiao do badge de posicao (imagem/boleta.png) e captura
// uma referencia de bitmap POR QUANTIDADE EXATA (vazio, comprado 1..N,
// vendido 1..N), fazendo o operador ir contruindo a posicao 1 contrato de
// cada vez em cada lado.
#pragma once

#include "config.h"
#include <string>

// devolve false se o operador cancelar (ESC) em algum passo.
bool rodarCalibracao(Calibracao& out);

// pergunta (uma vez, no inicio da sessao) se o Replay esta' ligado agora
// -- so' pra interpretar certo o clique abaixo -- e se quer reancorar
// rapido a POSICAO do badge (reaproveita as referencias ja calibradas,
// so' atualiza ONDE olhar na tela, nao refaz a construcao de 1..N
// contratos; util quando o Profit reabre em lugar diferente). So' mexe
// na posicao BASE de 'cal' (cal.regiaoBadge) se o operador confirmar que
// a leitura resultante bate com o que aparece na tela; opcionalmente
// salva a atualizacao em 'caminhoCalibracao'.
//
// NAO decide mais "vai operar com Replay ligado ou desligado" -- achado
// ao vivo, 15/09/2026: o dono liga o debug/rodar ainda no Replay (antes
// do pregao) e desliga o Replay NO MEIO da mesma sessao (quando o
// mercado abre), sem reiniciar o programa. Uma decisao fixa por sessao
// nao aguenta isso. Em vez disso, main.cpp vigia as DUAS posicoes
// possiveis (com e sem Replay, ver Calibracao::deslocamentoReplayY) ao
// mesmo tempo e usa automaticamente qual delas bater com alguma
// referencia -- funciona ligando/desligando o Replay a qualquer momento,
// sem precisar perguntar nem saber em qual estado esta'.
void prepararRegiaoDeLeitura(Calibracao& cal, const std::string& caminhoCalibracao);

// a OUTRA posicao possivel do badge (a que nao e' cal.regiaoBadge) --
// se calibradoComReplayLigado, regiaoBadge e' a posicao COM Replay e
// isso devolve a posicao SEM; senao e' o contrario. So' faz sentido
// chamar se cal.temReplay.
RegiaoTela regiaoAlternativaReplay(const Calibracao& cal);
