// config.h -- calibracao salva em disco: duas regioes da tela do badge de
// posicao (ver imagem/boleta.png) e as referencias de bitmap coletadas na
// sessao de calibracao.
//
// Duas regioes, nao uma so' (mudanca de 11/09/2026, achado ao vivo: 1V ->
// 2V nao estava disparando reforco):
//   regiaoBadge -- o badge INTEIRO (numero + letra). So' serve pra
//                  perceber "mudou" e pra decidir se ESTA' FLAT (comparado
//                  contra refVazioBadge) -- vazio tem aparencia bem
//                  diferente de "tem alguma coisa", entao o badge inteiro
//                  funciona bem pra essa distincao.
//   regiaoLetra -- so' a LETRA (C/V) dentro do badge, sem o numero. O
//                  numero muda de forma quando a quantidade muda (1 -> 2
//                  -> 3...), o que fazia o badge inteiro "1V" parecer
//                  diferente demais de "2V" pra bater com a referencia.
//                  A letra sozinha nao muda com a quantidade, entao serve
//                  pra decidir o LADO (compra/venda) sem se confundir com
//                  reforco.
#pragma once

#include "captura_tela.h"
#include <string>
#include <vector>

struct Calibracao {
    RegiaoTela regiaoBadge;
    RegiaoTela regiaoLetra;

    std::vector<BYTE> refVazioBadge;  // badge inteiro, posicao flat
    std::vector<BYTE> refCompraLetra; // so' a letra, posicao comprada
    std::vector<BYTE> refVendaLetra;  // so' a letra, posicao vendida

    long long toleranciaVazio = 0; // pro badge inteiro vs refVazioBadge
    long long toleranciaLado = 0;  // pra letra vs refCompraLetra/refVendaLetra
};

// grava 2 arquivos: "<caminhoBase>" (texto, regioes+tolerancias) e
// "<caminhoBase>.refs" (binario, os 3 bitmaps de referencia).
bool salvarCalibracao(const Calibracao& c, const std::string& caminhoBase);
bool carregarCalibracao(Calibracao& c, const std::string& caminhoBase);
