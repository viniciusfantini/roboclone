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
// No final, se a calibracao tiver suporte a Replay, pergunta em qual
// janela vai operar/debugar A PARTIR DE AGORA (com ou sem Replay) e
// devolve a REGIAO A USAR pro resto da sessao (ja' com o deslocamento
// aplicado, se for o caso). Essa decisao e' FIXA pra sessao inteira --
// se trocar de janela no meio (ex.: desligar o Replay durante o
// debug/rodar), reinicie o programa pra responder de novo (achado ao
// vivo, 15/09/2026: uma tentativa de vigiar as duas posicoes ao mesmo
// tempo, sem perguntar, tinha um bug real -- quando o Replay liga/
// desliga as DUAS regioes mudam no mesmo instante, e so' uma delas era
// conferida, perdendo a leitura da outra. Uma pergunta fixa e' mais
// simples e previsivel).
RegiaoTela prepararRegiaoDeLeitura(Calibracao& cal, const std::string& caminhoCalibracao);
