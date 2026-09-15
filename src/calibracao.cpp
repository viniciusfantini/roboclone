#include "calibracao.h"
#include "entrada.h"
#include "captura_tela.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

constexpr int NIVEL_MAXIMO_PADRAO = 5;

RegiaoTela regiaoDeDoisPontos(POINT a, POINT b) {
    RegiaoTela r;
    r.x = std::min(a.x, b.x);
    r.y = std::min(a.y, b.y);
    r.largura = std::max(1L, std::abs(b.x - a.x));
    r.altura = std::max(1L, std::abs(b.y - a.y));
    return r;
}

long long diferencaEntre(const std::vector<BYTE>& a, const std::vector<BYTE>& b) {
    if (a.size() != b.size()) return 0;
    long long soma = 0;
    for (size_t i = 0; i < a.size(); ++i) soma += std::abs((int)a[i] - (int)b[i]);
    return soma;
}

int perguntarNivelMaximo() {
    std::printf("\nAte' quantos contratos calibrar de cada lado? (ex.: 5 calibra 1..5\n");
    std::printf("comprado E 1..5 vendido). ENTER pra usar o padrao (%d): ", NIVEL_MAXIMO_PADRAO);
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    if (linha.empty()) return NIVEL_MAXIMO_PADRAO;
    int valor = std::atoi(linha.c_str());
    return valor > 0 ? valor : NIVEL_MAXIMO_PADRAO;
}

} // namespace

bool rodarCalibracao(Calibracao& out) {
    std::printf("== Calibracao ==\n");
    std::printf("Antes de comecar: deixe a janela do Profit aberta, mostrando o\n");
    std::printf("painel com o indicador de posicao \"Qtd\" (o badge tipo \"1C\"/\"2V\"\n");
    std::printf("-- ver imagem/boleta.png), do jeito que vai ficar durante o pregao.\n");
    std::printf("Aponte SO' pro badge \"Qtd\" -- nao inclua campos vizinhos que mudam\n");
    std::printf("sozinhos com o preco (ex.: \"Resultado\", \"Res. Aberto\"), senao\n");
    std::printf("qualquer variacao de preco vira um falso reforco.\n");

    POINT b1 = aguardarClique("canto SUPERIOR ESQUERDO do badge de posicao (numero + letra)");
    if (b1.x < 0 && b1.y < 0) return false;
    POINT b2 = aguardarClique("canto INFERIOR DIREITO do badge");
    if (b2.x < 0 && b2.y < 0) return false;
    out.regiaoBadge = regiaoDeDoisPontos(b1, b2);

    int nivelMaximo = perguntarNivelMaximo();

    CapturaRegiao cap(out.regiaoBadge);
    out.referencias.clear();

    std::printf("\nAgora vamos construir a posicao 1 contrato de cada vez, dos dois\n");
    std::printf("lados. Use a conta SIMULADORA -- isso faz operacao de verdade.\n");

    aguardarEnter("deixe a posicao ZERADA/FLAT agora");
    if (!cap.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
    out.referencias.push_back({0, cap.pixelsBrutos()});

    for (int i = 1; i <= nivelMaximo; ++i) {
        char instrucao[160];
        std::snprintf(instrucao, sizeof(instrucao),
                      "compre mais 1 contrato a mercado (fique comprado, total %d) e confirme", i);
        aguardarEnter(instrucao);
        if (!cap.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
        out.referencias.push_back({i, cap.pixelsBrutos()});
    }

    aguardarEnter("zere a posicao de teste (fique FLAT de novo) e confirme");

    for (int i = 1; i <= nivelMaximo; ++i) {
        char instrucao[160];
        std::snprintf(instrucao, sizeof(instrucao),
                      "venda mais 1 contrato a mercado (fique vendido, total %d) e confirme", i);
        aguardarEnter(instrucao);
        if (!cap.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
        out.referencias.push_back({-i, cap.pixelsBrutos()});
    }

    std::printf("\n>> pode zerar a posicao de teste agora, a calibracao ja capturou o que precisava.\n");

    // tolerancia = uma fracao da menor diferenca entre QUAISQUER duas
    // referencias capturadas (as mais parecidas entre si, ex.: 3C vs 4C,
    // sao o par mais critico) -- conservador o bastante pra nao confundir
    // niveis vizinhos, generoso o bastante pra aguentar variacao pequena
    // de rendering entre capturas.
    long long menor = -1;
    int idxA = -1, idxB = -1;
    for (size_t i = 0; i < out.referencias.size(); ++i) {
        for (size_t j = i + 1; j < out.referencias.size(); ++j) {
            long long d = diferencaEntre(out.referencias[i].bitmap, out.referencias[j].bitmap);
            if (menor < 0 || d < menor) { menor = d; idxA = out.referencias[i].quantidade; idxB = out.referencias[j].quantidade; }
        }
    }
    out.tolerancia = menor / 3;

    std::printf("\n== Calibracao concluida ==\n");
    std::printf("regiao badge: x=%d y=%d %dx%d\n", out.regiaoBadge.x, out.regiaoBadge.y,
                out.regiaoBadge.largura, out.regiaoBadge.altura);
    std::printf("referencias capturadas: %zu (vazio + 1..%d comprado + 1..%d vendido)\n",
                out.referencias.size(), nivelMaximo, nivelMaximo);
    std::printf("par mais parecido: quantidade %d vs %d, diferenca=%lld -> tolerancia=%lld\n",
                idxA, idxB, menor, out.tolerancia);

    if (menor < 30) {
        std::printf(">> AVISO: duas referencias ficaram muito parecidas entre si (quantidade %d e\n"
                    ">> %d, diferenca=%lld) -- pode confundir esses dois niveis. Confira se apontou\n"
                    ">> certo pro badge, ou recalibre se a leitura sair errada no modo debug.\n",
                    idxA, idxB, menor);
    }

    return true;
}
