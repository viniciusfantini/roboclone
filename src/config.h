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
// Removido suporte especifico a modo Replay (15/09/2026, pedido do dono):
// em vez de perguntar/medir/lembrar se o Replay esta' ligado, o programa
// agora LOCALIZA o badge sozinho procurando numa area ampla ao redor da
// ultima posicao conhecida (ver calibracao::localizarBadge) -- a
// referencia "vazio" (o "-" que aparece quando a posicao esta' flat) e'
// so' mais uma referencia normal nessa busca, tao boa quanto qualquer
// "1C"/"2V" pra achar onde o badge esta' agora, nao importa se foi o
// Replay, o zoom ou so' a janela que deslocou tudo.
#pragma once

#include "captura_tela.h"
#include <string>
#include <vector>

struct ReferenciaBadge {
    int quantidade; // 0 = vazio/flat, +N = comprado N, -N = vendido N
    std::vector<BYTE> bitmap;
};

struct Calibracao {
    RegiaoTela regiaoBadge; // ultima posicao conhecida -- usada como CENTRO da busca, nao precisa ser exata
    std::vector<ReferenciaBadge> referencias;
    long long tolerancia = 0;
};

// grava 2 arquivos: "<caminhoBase>" (texto, regiao+tolerancia+contagem) e
// "<caminhoBase>.refs" (binario, as referencias).
bool salvarCalibracao(const Calibracao& c, const std::string& caminhoBase);
bool carregarCalibracao(Calibracao& c, const std::string& caminhoBase);
