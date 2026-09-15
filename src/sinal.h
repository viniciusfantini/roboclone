// sinal.h -- logica pura de classificacao de sinal (sem tela, sem Win32).
//
// Mudanca de design (15/09/2026, a pedido do dono): antes so' distinguia
// LADO (comprado/vendido/flat), sem saber a quantidade exata -- qualquer
// mudanca no badge mantendo o mesmo lado virava "reforco" (manda mais um
// na mesma direcao). Problema: uma REDUCAO parcial (ex.: 7C -> 6C, uma
// venda parcial que ainda deixa comprado) tambem mantem o mesmo lado --
// e' lida errado como reforco (mandaria COMPRA de novo, dobrando o erro,
// em vez de mandar VENDA pra reduzir igual aconteceu de verdade). Agora a
// posicao e' um INTEIRO com sinal (positivo = comprado N, negativo =
// vendido N, 0 = flat), lido comparando o badge contra referencias
// calibradas POR QUANTIDADE EXATA (1C, 2C, ..., 1V, 2V, ... -- ver
// calibracao.h) em vez de so' 2 (compra/venda). Isso tambem resolve o
// problema original do digito mudando de forma (11/09/2026): agora cada
// quantidade TEM sua propria referencia, entao "2C" nao precisa mais
// "parecer" com "1C".
//
// A funcao transicao() so' faz a parte pura: dado a posicao ANTERIOR e a
// NOVA (inteiros com sinal, ja classificados por quem le a tela), devolve
// a lista de comandos a mandar, um por unidade de diferenca. Normalmente
// 1 evento (mudanca de 1 contrato, o caso comum); mais de 1 se a leitura
// pular mais de um degrau de uma vez (ex.: perdeu um frame no meio) ou no
// caso de virada direta (comprado <-> vendido sem passar por flat
// detectado -- fecha tudo e abre do outro lado, um comando por unidade).
#pragma once

#include <vector>
#include <cstdlib>

enum class Comando { Compra, Venda, Zerar };
enum class TipoEvento { Abertura, Reforco, ReducaoParcial, Zeragem };

struct Evento {
    Comando comando;
    TipoEvento tipo;
};

// dado que o badge mudou de 'anterior' pra 'novo' (chamador so' invoca
// quando de fato mudou), devolve os eventos a mandar, na ordem. Positivo
// = comprado N, negativo = vendido N, 0 = flat.
inline std::vector<Evento> transicao(int anterior, int novo) {
    std::vector<Evento> eventos;
    if (anterior == novo) return eventos;

    if (anterior == 0) {
        // abertura, pode abrir direto com mais de 1 se pulou um degrau.
        Comando cmd = (novo > 0) ? Comando::Compra : Comando::Venda;
        int n = std::abs(novo);
        for (int i = 0; i < n; ++i) {
            eventos.push_back({cmd, i == 0 ? TipoEvento::Abertura : TipoEvento::Reforco});
        }
        return eventos;
    }

    if (novo == 0) {
        eventos.push_back({Comando::Zerar, TipoEvento::Zeragem});
        return eventos;
    }

    bool mesmoSinal = (anterior > 0) == (novo > 0);
    if (mesmoSinal) {
        int antesAbs = std::abs(anterior), novoAbs = std::abs(novo);
        Comando cmdMesmaDirecao = (anterior > 0) ? Comando::Compra : Comando::Venda;
        Comando cmdReducao = (anterior > 0) ? Comando::Venda : Comando::Compra;
        if (novoAbs > antesAbs) {
            for (int i = 0; i < novoAbs - antesAbs; ++i) eventos.push_back({cmdMesmaDirecao, TipoEvento::Reforco});
        } else {
            for (int i = 0; i < antesAbs - novoAbs; ++i) eventos.push_back({cmdReducao, TipoEvento::ReducaoParcial});
        }
        return eventos;
    }

    // virada direta (comprado <-> vendido sem passar por flat detectado)
    // -- fecha o lado antigo e abre o novo do zero, igual b3_money_copy
    // (achado 18/08/2026 la').
    eventos.push_back({Comando::Zerar, TipoEvento::Zeragem});
    Comando cmdAbertura = (novo > 0) ? Comando::Compra : Comando::Venda;
    int n = std::abs(novo);
    for (int i = 0; i < n; ++i) {
        eventos.push_back({cmdAbertura, i == 0 ? TipoEvento::Abertura : TipoEvento::Reforco});
    }
    return eventos;
}

inline const char* nomeComando(Comando c) {
    switch (c) {
        case Comando::Compra: return "COMPRA";
        case Comando::Venda:  return "VENDA";
        case Comando::Zerar:  return "ZERAR";
    }
    return "?";
}

// rotulo pro modo debug (bloco de notas): C/V abertura, CC/VV reforco,
// c/v reducao parcial (minusculo -- ainda nao zerou, so' reduziu; note
// que a TECLA enviada nesse caso e' a OPOSTA ao lado da posicao, porque
// reduzir um comprado manda venda e vice-versa), Z zeragem total.
inline const char* rotuloDebug(const Evento& e) {
    switch (e.tipo) {
        case TipoEvento::Abertura: return (e.comando == Comando::Compra) ? "C" : "V";
        case TipoEvento::Reforco:  return (e.comando == Comando::Compra) ? "CC" : "VV";
        case TipoEvento::ReducaoParcial: return (e.comando == Comando::Compra) ? "c" : "v";
        case TipoEvento::Zeragem: return "Z";
    }
    return "?";
}
