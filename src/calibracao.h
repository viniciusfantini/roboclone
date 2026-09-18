// calibracao.h -- fluxo interativo (console) que descobre, por clique do
// operador, a regiao do badge de posicao (imagem/boleta.png) e captura
// uma referencia de bitmap POR QUANTIDADE EXATA (vazio -- o "-" que
// aparece quando a posicao esta flat --, comprado 1..N, vendido 1..N),
// fazendo o operador ir construindo a posicao 1 contrato de cada vez em
// cada lado.
#pragma once

#include "config.h"
#include <string>

// devolve false se o operador cancelar (ESC) em algum passo.
bool rodarCalibracao(Calibracao& out);

struct ResultadoBusca {
    RegiaoTela regiao;
    int quantidade = 0;
    long long diferenca = -1; // -1 = nao achou nada dentro da tolerancia
};

// procura, numa area ao redor de 'centro (raio 'raioPx pixels em x e
// y), a posicao que da a MENOR diferenca de bitmap contra QUALQUER
// referencia calibrada. Nao filtra por tolerancia -- quem chama decide
// se 'diferenca e boa o bastante (compare com cal.tolerancia).
//
// Essa e a base de como o programa acha o badge sozinho, sem perguntar
// nada sobre Replay/zoom/janela ter mexido (mudanca de 15/09/2026,
// pedido do dono): a referencia "vazio" (o "-" que aparece quando a
// posicao esta flat) e so mais uma referencia normal aqui, tao boa
// quanto "1C"/"2V" pra localizar onde o badge esta AGORA.
ResultadoBusca buscarBadge(POINT centro, int largura, int altura,
                           const std::vector<ReferenciaBadge>& referencias, int raioPx);

// localiza o badge automaticamente, sem perguntar nada sobre Replay:
// busca numa area ampla ao redor de cal.regiaoBadge (a ultima posicao
// conhecida). 'origemEsperada e a janela escolhida pelo operador como
// ORIGEM (ver janela_alvo.h) -- se o que a busca achar pertencer a uma
// janela DIFERENTE (ex.: outra janela do Profit aberta por perto,
// sobrepondo a area de busca), o achado e rejeitado como se nao tivesse
// achado nada, pra nao ler a conta errada. Se nao achar nada (nem
// pertencente a origemEsperada), pede UM clique aproximado (nao precisa
// ser exato) DENTRO da janela de origem como ponto de partida pra tentar
// de novo. Atualiza cal.regiaoBadge com a posicao achada e pergunta se
// quer salvar pra proxima vez. Devolve a regiao ativa pra usar na
// sessao; se nem com o clique achar nada valido, devolve a ultima regiao
// conhecida mesmo assim (com aviso -- quem le vai continuar tomando
// "nao bateu" ate recalibrar).
RegiaoTela localizarBadge(Calibracao& cal, const std::string& caminhoCalibracao, HWND origemEsperada);
