// calibracao.h -- fluxo interativo (console) que descobre, por clique do
// operador, a regiao do badge de posicao (imagem/boleta.png) e captura 3
// referencias de bitmap (vazio/comprado/vendido) fazendo o operador
// colocar a conta simuladora em cada um desses 3 estados.
#pragma once

#include "config.h"

// devolve false se o operador cancelar (ESC) em algum passo.
bool rodarCalibracao(Calibracao& out);
