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
// -- ver config.h, "Suporte a modo Replay" -- e se quer reancorar rapido
// a POSICAO do badge (reaproveita as referencias ja calibradas, so'
// atualiza ONDE olhar na tela, nao refaz a construcao de 1..N contratos;
// util quando o Profit reabre em lugar diferente). So' mexe na posicao
// BASE de 'cal' se o operador confirmar que a leitura resultante bate
// com o que aparece na tela; opcionalmente salva a atualizacao em
// 'caminhoCalibracao'. Devolve a REGIAO A USAR pro resto da sessao (ja'
// com o deslocamento do Replay somado, se for o caso).
RegiaoTela prepararRegiaoDeLeitura(Calibracao& cal, const std::string& caminhoCalibracao);
