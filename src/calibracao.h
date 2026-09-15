// calibracao.h -- fluxo interativo (console) que descobre, por clique do
// operador, a regiao do badge de posicao (imagem/boleta.png) e captura
// uma referencia de bitmap POR QUANTIDADE EXATA (vazio, comprado 1..N,
// vendido 1..N), fazendo o operador ir contruindo a posicao 1 contrato de
// cada vez em cada lado.
#pragma once

#include "config.h"

// devolve false se o operador cancelar (ESC) em algum passo.
bool rodarCalibracao(Calibracao& out);
