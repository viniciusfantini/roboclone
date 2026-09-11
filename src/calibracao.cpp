#include "calibracao.h"
#include "entrada.h"
#include "captura_tela.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>

namespace {

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

} // namespace

bool rodarCalibracao(Calibracao& out) {
    std::printf("== Calibracao ==\n");
    std::printf("Antes de comecar: deixe a janela do Profit aberta, mostrando o\n");
    std::printf("painel com o indicador de posicao \"Qtd\" (o badge tipo \"1C\"/\"2V\"\n");
    std::printf("-- ver imagem/boleta.png), do jeito que vai ficar durante o pregao.\n");

    POINT b1 = aguardarClique("canto SUPERIOR ESQUERDO do badge de posicao inteiro (numero + letra)");
    if (b1.x < 0 && b1.y < 0) return false;
    POINT b2 = aguardarClique("canto INFERIOR DIREITO do badge inteiro");
    if (b2.x < 0 && b2.y < 0) return false;
    out.regiaoBadge = regiaoDeDoisPontos(b1, b2);

    std::printf("\nAgora so' a LETRA (C ou V), sem o numero -- normalmente o\n");
    std::printf("caractere mais a direita dentro do badge.\n");

    POINT l1 = aguardarClique("canto SUPERIOR ESQUERDO da LETRA (C/V), sem pegar o numero");
    if (l1.x < 0 && l1.y < 0) return false;
    POINT l2 = aguardarClique("canto INFERIOR DIREITO da LETRA");
    if (l2.x < 0 && l2.y < 0) return false;
    out.regiaoLetra = regiaoDeDoisPontos(l1, l2);

    CapturaRegiao capBadge(out.regiaoBadge);
    CapturaRegiao capLetra(out.regiaoLetra);

    std::printf("\nAgora vamos capturar as referencias. Isso PRECISA ser feito na\n");
    std::printf("conta que vai ficar de fato flat/comprada/vendida nesses passos --\n");
    std::printf("use a conta SIMULADORA pra nao correr risco nenhum.\n");

    aguardarEnter("deixe a posicao ZERADA/FLAT agora");
    if (!capBadge.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
    out.refVazioBadge = capBadge.pixelsBrutos();
    std::vector<BYTE> badgeVazio = out.refVazioBadge;

    aguardarEnter("compre 1 contrato a mercado (fique COMPRADO) e confirme quando o badge aparecer");
    if (!capBadge.capturar() || !capLetra.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
    std::vector<BYTE> badgeCompra = capBadge.pixelsBrutos();
    out.refCompraLetra = capLetra.pixelsBrutos();

    aguardarEnter("zere e venda 1 contrato a mercado (fique VENDIDO) e confirme quando o badge aparecer");
    if (!capBadge.capturar() || !capLetra.capturar()) { std::printf(">> falha ao capturar a tela.\n"); return false; }
    std::vector<BYTE> badgeVenda = capBadge.pixelsBrutos();
    out.refVendaLetra = capLetra.pixelsBrutos();

    std::printf("\n>> pode zerar a posicao de teste agora, a calibracao ja capturou o que precisava.\n");

    long long dVazioCompra = diferencaEntre(badgeVazio, badgeCompra);
    long long dVazioVenda = diferencaEntre(badgeVazio, badgeVenda);
    out.toleranciaVazio = std::min(dVazioCompra, dVazioVenda) / 3;

    long long dLetras = diferencaEntre(out.refCompraLetra, out.refVendaLetra);
    out.toleranciaLado = dLetras / 3;

    std::printf("\n== Calibracao concluida ==\n");
    std::printf("regiao badge: x=%d y=%d %dx%d\n", out.regiaoBadge.x, out.regiaoBadge.y,
                out.regiaoBadge.largura, out.regiaoBadge.altura);
    std::printf("regiao letra: x=%d y=%d %dx%d\n", out.regiaoLetra.x, out.regiaoLetra.y,
                out.regiaoLetra.largura, out.regiaoLetra.altura);
    std::printf("diferenca badge vazio<->compra=%lld, vazio<->venda=%lld -> toleranciaVazio=%lld\n",
                dVazioCompra, dVazioVenda, out.toleranciaVazio);
    std::printf("diferenca letra compra<->venda=%lld -> toleranciaLado=%lld\n", dLetras, out.toleranciaLado);

    if (std::min(dVazioCompra, dVazioVenda) < 30) {
        std::printf(">> AVISO: badge vazio ficou muito parecido com badge ocupado -- confira se\n"
                    ">> apontou certo pro badge de posicao inteiro.\n");
    }
    if (dLetras < 30) {
        std::printf(">> AVISO: a letra de compra ficou muito parecida com a de venda -- confira se\n"
                    ">> a regiao da LETRA nao esta pegando o numero junto, ou se nao ficou torta.\n");
    }

    return true;
}
