// sinal.h -- logica pura de classificacao de sinal (sem tela, sem Win32).
//
// Mudanca de design (11/09/2026, a pedido do dono): em vez de inferir a
// posicao contando o LADO de cada execucao na grade (frágil se uma saida
// virar varias execucoes parciais em vez de uma so' do tamanho da posicao
// inteira), o estado (Flat/Comprado/Vendido) agora vem DIRETO do badge de
// posicao do Profit (ex.: "1C", "2V" -- ver imagem/boleta.png), lido por
// comparacao de bitmap (ver captura_tela.h). Isso e' a fonte da verdade:
// so' fica Flat quando a posicao de fato zerou, nao importa quantas
// execucoes levou.
//
// A funcao transicao() so' faz a parte pura: dado o estado ANTERIOR e o
// estado NOVO (ja classificados por quem le a tela), devolve a lista de
// comandos a mandar. Normalmente 1 evento; 2 no caso raro de virada direta
// (Comprado -> Vendido sem passar por Flat detectado) -- mesma convencao
// do b3_money_copy: fecha o lado antigo (ZERAR) e abre o novo, como dois
// comandos separados.
#pragma once

#include <vector>

enum class EstadoPosicao { Flat, Comprado, Vendido };
enum class Comando { Compra, Venda, Zerar };

struct Evento {
    Comando comando;
    bool reforco; // true = mesma direcao de uma posicao ja aberta (nao abertura, nem zeragem)
};

// dado que o badge mudou de 'anterior' pra 'novo' (chamador so' invoca
// quando de fato mudou), devolve os eventos a mandar, na ordem.
inline std::vector<Evento> transicao(EstadoPosicao anterior, EstadoPosicao novo) {
    std::vector<Evento> eventos;

    if (anterior == novo) {
        // mesmo lado non-flat, badge mudou (ex.: "1C" -> "2C") = reforco.
        if (novo == EstadoPosicao::Comprado) eventos.push_back({Comando::Compra, true});
        else if (novo == EstadoPosicao::Vendido) eventos.push_back({Comando::Venda, true});
        return eventos; // anterior==novo==Flat nao deveria chamar transicao()
    }

    if (anterior == EstadoPosicao::Flat) {
        eventos.push_back({novo == EstadoPosicao::Comprado ? Comando::Compra : Comando::Venda, false});
        return eventos;
    }

    if (novo == EstadoPosicao::Flat) {
        eventos.push_back({Comando::Zerar, false});
        return eventos;
    }

    // virada direta (Comprado <-> Vendido sem passar por Flat detectado)
    // -- fecha o lado antigo e abre o novo, dois comandos (igual
    // b3_money_copy, achado 18/08/2026 la').
    eventos.push_back({Comando::Zerar, false});
    eventos.push_back({novo == EstadoPosicao::Comprado ? Comando::Compra : Comando::Venda, false});
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

// rotulo de 1-2 letras pedido pro modo debug (bloco de notas): C/V pra
// abertura, CC/VV pra reforco, Z pra zeragem/alvo.
inline const char* rotuloDebug(const Evento& e) {
    switch (e.comando) {
        case Comando::Compra: return e.reforco ? "CC" : "C";
        case Comando::Venda:  return e.reforco ? "VV" : "V";
        case Comando::Zerar:  return "Z";
    }
    return "?";
}
