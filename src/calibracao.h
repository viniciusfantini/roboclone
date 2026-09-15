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

// pergunta se quer reancorar rapido a POSICAO do badge (reaproveita as
// referencias ja calibradas -- so' atualiza ONDE olhar na tela, nao
// refaz a construcao de 1..N contratos). Util quando o Profit reabre em
// lugar diferente. Pode ser feito com o Replay ligado antes do pregao
// abrir, mostrando um estado conhecido (ex. "1C") pra conferir contra um
// estado que nao e' flat. So' mexe em 'cal' se o operador confirmar que
// a leitura resultante bate com o que aparece na tela; opcionalmente
// salva a atualizacao em 'caminhoCalibracao'. Sempre devolve true
// (responder "nao" a qualquer pergunta so' significa "nao mexeu em
// nada", nunca e' erro).
bool confirmarOuReancorarPosicao(Calibracao& cal, const std::string& caminhoCalibracao);
