// main.cpp -- roboclone: le o indicador de posicao ("Qtd", ex.: "1C"/
// "2V" -- ver imagem/boleta.png) do Profit na conta de ORIGEM (via
// comparacao de bitmap, sem OCR) e manda o comando equivalente (compra/
// venda/zerar) por atalho de teclado pra uma segunda janela do Profit,
// numa conta SIMULADORA.
//
// Uso:
//   roboclone.exe calibrar   -- descobre por clique as 2 regioes (badge
//                                inteiro + so' a letra) e captura as
//                                referencias (vazio/comprado/vendido)
//   roboclone.exe debug      -- NAO manda atalho nenhum; escreve o rotulo
//                                lido (C/V/CC/VV/Z) num bloco de notas, pra
//                                conferir a leitura antes de confiar nela
//   roboclone.exe rodar      -- roda de verdade (manda atalho de verdade)
//   roboclone.exe testaratalho -- so' testa o envio do atalho numa janela
//                                escolhida, sem calibracao nem leitura de
//                                tela nenhuma (pra depurar se o ALT+C/V/A
//                                esta' chegando de verdade no Profit)
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <optional>

#include "config.h"
#include "calibracao.h"
#include "captura_tela.h"
#include "janela_alvo.h"
#include "atalho.h"
#include "sinal.h"
#include "escrever_texto.h"
#include "janela_teste.h"

namespace {

const char* CAMINHO_CALIBRACAO = "calibracao.cfg";
constexpr int INTERVALO_POLL_MS = 10;
constexpr int ESPERA_ACOMODAR_MS = 60;
constexpr int TAMANHO_MAXIMO_POSICAO_PADRAO = 5;

int perguntarDelayMs() {
    std::printf("\nEspacamento minimo entre comandos enviados, em ms (o Profit recusa com\n");
    std::printf("\"Alerta de envio duplicado\" se dois atalhos saem rapido demais -- 200ms\n");
    std::printf("resolveu no b3_money_copy). ENTER pra usar o padrao (%d): ", DELAY_MIN_ENTRE_COPIAS_MS_PADRAO);
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    if (linha.empty()) return DELAY_MIN_ENTRE_COPIAS_MS_PADRAO;
    int valor = std::atoi(linha.c_str());
    if (valor <= 0) {
        std::printf(">> valor invalido, usando o padrao (%d)\n", DELAY_MIN_ENTRE_COPIAS_MS_PADRAO);
        return DELAY_MIN_ENTRE_COPIAS_MS_PADRAO;
    }
    return valor;
}

int perguntarTamanhoMaximoPosicao() {
    std::printf("\nTamanho maximo de posicao permitido antes de PARAR de mandar atalho\n");
    std::printf("automatico (trava de seguranca contra loop -- ver README). ENTER pra\n");
    std::printf("usar o padrao (%d): ", TAMANHO_MAXIMO_POSICAO_PADRAO);
    std::fflush(stdout);
    std::string linha;
    std::getline(std::cin, linha);
    if (linha.empty()) return TAMANHO_MAXIMO_POSICAO_PADRAO;
    int valor = std::atoi(linha.c_str());
    if (valor <= 0) {
        std::printf(">> valor invalido, usando o padrao (%d)\n", TAMANHO_MAXIMO_POSICAO_PADRAO);
        return TAMANHO_MAXIMO_POSICAO_PADRAO;
    }
    return valor;
}

const char* nomeEstado(EstadoPosicao e) {
    switch (e) {
        case EstadoPosicao::Flat: return "FLAT";
        case EstadoPosicao::Comprado: return "COMPRADO";
        case EstadoPosicao::Vendido: return "VENDIDO";
    }
    return "?";
}

// classifica o estado atual: primeiro checa se o badge INTEIRO bate com
// "vazio" (flat); se nao bater, classifica o LADO usando so' a regiao da
// LETRA (nao se confunde com o numero mudando de forma no reforco).
std::optional<EstadoPosicao> classificar(CapturaRegiao& capBadge, CapturaRegiao& capLetra, const Calibracao& cal) {
    long long dVazio = capBadge.diferencaPara(cal.refVazioBadge);
    if (dVazio <= cal.toleranciaVazio) return EstadoPosicao::Flat;

    if (!capLetra.capturar()) return std::nullopt;
    long long dCompra = capLetra.diferencaPara(cal.refCompraLetra);
    long long dVenda = capLetra.diferencaPara(cal.refVendaLetra);

    if (dCompra <= cal.toleranciaLado && dCompra <= dVenda) return EstadoPosicao::Comprado;
    if (dVenda <= cal.toleranciaLado) return EstadoPosicao::Vendido;
    return std::nullopt;
}

POINT centroRegiao(const RegiaoTela& r) {
    return POINT{ r.x + r.largura / 2, r.y + r.altura / 2 };
}

// bloqueia ate' detectar uma mudanca de estado reconhecida (o badge mudou
// E a nova aparencia bate com uma das referencias calibradas). Avisa no
// console (sem travar) se o badge mudar pra algo nao reconhecido, OU se a
// janela que esta' fisicamente naquele pedaco de tela agora nao e' mais a
// janela de ORIGEM esperada (ex.: a janela de destino ficou por cima --
// sem essa checagem, leria o badge errado, podendo criar um loop lendo as
// proprias ordens que mandou).
EstadoPosicao aguardarProximoEstado(CapturaRegiao& capBadge, CapturaRegiao& capLetra,
                                     const Calibracao& cal, HWND origemEsperada) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(INTERVALO_POLL_MS));

        if (!capBadge.capturar()) continue;
        if (!capBadge.mudouDesdeUltimaCaptura()) continue;

        HWND atual = janelaNoPonto(centroRegiao(cal.regiaoBadge));
        if (atual != origemEsperada) {
            std::printf("[aviso] a janela na regiao calibrada da origem NAO e' mais a janela de "
                        "origem esperada (HWND=%p) -- leitura ignorada. Confira se a janela de "
                        "destino nao ficou por cima.\n", (void*)atual);
            continue;
        }

        // deixa o repaint acomodar antes de reler (evita pegar um frame no
        // meio da atualizacao) e reestabelece a baseline pra nao
        // re-disparar no proximo poll com o mesmo conteudo.
        std::this_thread::sleep_for(std::chrono::milliseconds(ESPERA_ACOMODAR_MS));
        capBadge.capturar();

        auto estado = classificar(capBadge, capLetra, cal);
        if (estado.has_value()) return *estado;

        std::printf("[aviso] badge mudou mas nao bateu com nenhuma referencia dentro da tolerancia "
                    "-- ignorado\n");
    }
}

bool carregarCalibracaoOuAvisar(Calibracao& cal) {
    if (carregarCalibracao(cal, CAMINHO_CALIBRACAO)) return true;
    std::fprintf(stderr, "ERRO: nao achei %s -- rode \"roboclone.exe calibrar\" primeiro.\n",
                 CAMINHO_CALIBRACAO);
    return false;
}

int modoCalibrar() {
    Calibracao cal;
    if (!rodarCalibracao(cal)) {
        std::printf("Calibracao cancelada, nada foi salvo.\n");
        return 1;
    }
    if (!salvarCalibracao(cal, CAMINHO_CALIBRACAO)) {
        std::fprintf(stderr, "ERRO: nao consegui salvar %s\n", CAMINHO_CALIBRACAO);
        return 1;
    }
    std::printf("Calibracao salva em %s (+ %s.refs)\n", CAMINHO_CALIBRACAO, CAMINHO_CALIBRACAO);
    return 0;
}

// atualiza o contador local de tamanho (so' pra exibir/debug -- a logica
// de comando nao depende dele, so' do lado). Compra/Venda com reforco=true
// soma 1; sem reforco (abertura) volta pra 1; Zerar volta pra 0.
void atualizarTamanho(int& tamanho, const Evento& evento) {
    if (evento.comando == Comando::Zerar) { tamanho = 0; return; }
    tamanho = evento.reforco ? (tamanho + 1) : 1;
}

int modoDebug() {
    Calibracao cal;
    if (!carregarCalibracaoOuAvisar(cal)) return 1;

    HWND origem = escolherJanelaPorClique("Profit da conta de ORIGEM (a que sera' lida)");
    if (!origem) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    HWND blocoDeNotas = escolherJanelaPorClique("bloco de notas (vai receber o rotulo lido: C/V/CC/VV/Z)");
    if (!blocoDeNotas) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    CapturaRegiao capBadge(cal.regiaoBadge);
    CapturaRegiao capLetra(cal.regiaoLetra);
    if (!capBadge.capturar() || !capLetra.capturar()) {
        std::fprintf(stderr, "ERRO: falha ao capturar as regioes calibradas. Recalibre.\n");
        return 1;
    }
    auto estadoInicial = classificar(capBadge, capLetra, cal);
    EstadoPosicao estadoAnterior = estadoInicial.value_or(EstadoPosicao::Flat);
    int tamanho = (estadoAnterior == EstadoPosicao::Flat) ? 0 : 1;
    std::printf("estado inicial: %s (tamanho assumido: %d)\n", nomeEstado(estadoAnterior), tamanho);

    std::printf("\nModo DEBUG -- NAO manda nenhum atalho, so' escreve no bloco de notas.\n");
    std::printf("Pode operar manualmente na conta de origem agora. CTRL+C pra sair.\n");

    while (true) {
        EstadoPosicao novo = aguardarProximoEstado(capBadge, capLetra, cal, origem);

        for (const Evento& evento : transicao(estadoAnterior, novo)) {
            atualizarTamanho(tamanho, evento);
            const char* rotulo = rotuloDebug(evento);

            SYSTEMTIME st;
            GetLocalTime(&st);
            char linha[80];
            std::snprintf(linha, sizeof(linha), "%02d:%02d:%02d.%03d %s (tam %d)",
                          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, rotulo, tamanho);

            std::printf("[debug] %s -> %s : rotulo %s, comando %s, tamanho agora %d\n",
                        nomeEstado(estadoAnterior), nomeEstado(novo), rotulo,
                        nomeComando(evento.comando), tamanho);

            if (!escreverLinha(blocoDeNotas, linha)) {
                std::fprintf(stderr, "[debug] falha ao escrever no bloco de notas (a janela ainda esta aberta?)\n");
            }
        }
        estadoAnterior = novo;
    }
}

int modoRodar() {
    Calibracao cal;
    if (!carregarCalibracaoOuAvisar(cal)) return 1;

    HWND origem = escolherJanelaPorClique("Profit da conta de ORIGEM (a que sera' lida)");
    if (!origem) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    HWND destino = escolherJanelaPorClique("Profit da conta SIMULADORA (destino dos atalhos)");
    if (!destino) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    if (origem == destino) {
        std::printf(">> aviso: origem e destino sao a MESMA janela (ok pra teste, mas em uso real\n"
                    ">> isso reage ao proprio envio -- confira antes de deixar rodando sozinho).\n");
    }

    definirEspacamentoMinimoMs(perguntarDelayMs());
    std::printf("espacamento minimo entre comandos: %dms\n", espacamentoMinimoAtualMs());

    int tamanhoMaximo = perguntarTamanhoMaximoPosicao();
    std::printf("tamanho maximo de posicao antes de parar o envio automatico: %d\n", tamanhoMaximo);

    CapturaRegiao capBadge(cal.regiaoBadge);
    CapturaRegiao capLetra(cal.regiaoLetra);
    if (!capBadge.capturar() || !capLetra.capturar()) {
        std::fprintf(stderr, "ERRO: falha ao capturar as regioes calibradas. Recalibre.\n");
        return 1;
    }
    auto estadoInicial = classificar(capBadge, capLetra, cal);
    EstadoPosicao estadoAnterior = estadoInicial.value_or(EstadoPosicao::Flat);
    int tamanho = (estadoAnterior == EstadoPosicao::Flat) ? 0 : 1;
    std::printf("estado inicial: %s (tamanho assumido: %d)\n", nomeEstado(estadoAnterior), tamanho);
    std::printf("\nRodando. Feche a janela de teste (ou CTRL+C aqui) pra sair.\n");

    // leitura + envio automatico rodam numa thread separada -- a thread
    // principal fica livre pra bombear mensagens da janela de teste
    // (Compra/Venda/Zerar), que manda o atalho manualmente a qualquer
    // momento, sem interromper a leitura.
    std::thread threadLeitura([&capBadge, &capLetra, &cal, origem, destino, estadoAnterior, tamanho, tamanhoMaximo]() mutable {
        while (true) {
            EstadoPosicao novo = aguardarProximoEstado(capBadge, capLetra, cal, origem);

            for (const Evento& evento : transicao(estadoAnterior, novo)) {
                atualizarTamanho(tamanho, evento);
                char tecla = (evento.comando == Comando::Compra) ? 'C' : (evento.comando == Comando::Venda) ? 'V' : 'A';
                std::printf("[sinal] %s -> %s : comando %s (ALT+%c), tamanho agora %d\n",
                            nomeEstado(estadoAnterior), nomeEstado(novo), nomeComando(evento.comando), tecla, tamanho);

                if (tamanho > tamanhoMaximo) {
                    std::printf("\n!!! TRAVA DE SEGURANCA !!! tamanho (%d) passou do maximo (%d) --\n"
                                "envio automatico PARADO (provavel loop: origem e destino reagindo\n"
                                "um ao outro). Os botoes da janela de teste continuam funcionando\n"
                                "pra voce zerar manualmente. Reinicie o \"rodar\" depois de conferir.\n\n",
                                tamanho, tamanhoMaximo);
                    return;
                }

                enviarAltTeclaComEspacamento(destino, tecla);
            }
            estadoAnterior = novo;
        }
    });
    threadLeitura.detach();

    rodarJanelaTeste(destino);
    return 0;
}

int modoTestarAtalho() {
    HWND alvo = escolherJanelaPorClique("janela de teste (a que deve receber o ALT+C/V/A)");
    if (!alvo) {
        std::printf("Nenhuma janela escolhida, saindo.\n");
        return 1;
    }

    definirEspacamentoMinimoMs(perguntarDelayMs());
    std::printf("espacamento minimo entre comandos (so' vale pro c/v/a, nao pro fc/fv/fa): %dms\n",
                espacamentoMinimoAtualMs());

    std::printf("\nDigite um comando e ENTER:\n");
    std::printf("  c / v / a      -> manda via PostMessage (sem foco)\n");
    std::printf("  fc / fv / fa   -> manda via SendInput (rouba o foco e devolve)\n");
    std::printf("  sair           -> termina\n");

    std::string linha;
    while (true) {
        std::printf("> ");
        std::fflush(stdout);
        if (!std::getline(std::cin, linha)) break;
        if (linha == "sair") break;

        if (linha == "c") enviarAltTeclaComEspacamento(alvo, 'C');
        else if (linha == "v") enviarAltTeclaComEspacamento(alvo, 'V');
        else if (linha == "a") enviarAltTeclaComEspacamento(alvo, 'A');
        else if (linha == "fc") enviarAltTeclaComFoco(alvo, 'C');
        else if (linha == "fv") enviarAltTeclaComFoco(alvo, 'V');
        else if (linha == "fa") enviarAltTeclaComFoco(alvo, 'A');
        else std::printf("nao entendi \"%s\" -- use c/v/a, fc/fv/fa ou sair\n", linha.c_str());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("uso: %s calibrar|debug|rodar|testaratalho\n", argv[0]);
        return 1;
    }
    if (std::strcmp(argv[1], "calibrar") == 0) return modoCalibrar();
    if (std::strcmp(argv[1], "debug") == 0) return modoDebug();
    if (std::strcmp(argv[1], "rodar") == 0) return modoRodar();
    if (std::strcmp(argv[1], "testaratalho") == 0) return modoTestarAtalho();

    std::printf("comando desconhecido: %s\nuso: %s calibrar|debug|rodar|testaratalho\n", argv[1], argv[0]);
    return 1;
}
