// config.h -- calibracao salva em disco: a regiao do badge de posicao
// (ver imagem/boleta.png) e uma referencia de bitmap POR QUANTIDADE EXATA
// (vazio=0, comprado 1..N=+1..+N, vendido 1..N=-1..-N).
//
// Mudanca de design (15/09/2026): antes eram so' 3 referencias (vazio/
// compra-generico/venda-generico, comparando so' a LETRA pra nao se
// confundir com o digito mudando de forma). Agora, com uma referencia
// exata por quantidade, comparar o BADGE INTEIRO funciona direto -- nao
// precisa mais separar regiao da letra.
//
// Suporte a modo Replay do Profit (15/09/2026): o Replay insere uma barra
// amarela no topo da janela, empurrando todo o layout (inclusive o badge
// de posicao) pra baixo por um deslocamento fixo -- medido ao vivo em
// 24px numa maquina, mas guardado como algo MEDIDO na calibracao (nao
// chumbado no codigo), porque pode variar com DPI/tema/monitor. So' o
// deslocamento em Y e' guardado -- decidir se o Replay esta' ligado ou
// nao e' perguntado ao operador uma vez por sessao (ver
// calibracao::prepararRegiaoDeLeitura), nao detectado ao vivo por cor
// (simplificado em 15/09/2026: Replay so' e' usado antes do pregao
// abrir, nunca liga/desliga no meio de uma sessao de verdade, entao
// checar isso a cada poll era complexidade sem necessidade).
#pragma once

#include "captura_tela.h"
#include <string>
#include <vector>

struct ReferenciaBadge {
    int quantidade; // 0 = vazio/flat, +N = comprado N, -N = vendido N
    std::vector<BYTE> bitmap;
};

struct Calibracao {
    RegiaoTela regiaoBadge;
    std::vector<ReferenciaBadge> referencias;
    long long tolerancia = 0;

    // deslocamento pro modo Replay -- opcional (temReplay=false se o
    // operador pulou essa parte da calibracao).
    bool temReplay = false;
    int deslocamentoReplayY = 0; // quanto o badge desce quando o Replay liga
};

// grava 2 arquivos: "<caminhoBase>" (texto, regiao+tolerancia+contagem) e
// "<caminhoBase>.refs" (binario, as referencias).
bool salvarCalibracao(const Calibracao& c, const std::string& caminhoBase);
bool carregarCalibracao(Calibracao& c, const std::string& caminhoBase);
