#include "config.h"
#include <fstream>
#include <map>
#include <cstdlib>

namespace {
std::string caminhoRefs(const std::string& caminhoBase) { return caminhoBase + ".refs"; }
}

bool salvarCalibracao(const Calibracao& c, const std::string& caminhoBase) {
    {
        std::ofstream f(caminhoBase, std::ios::trunc);
        if (!f) return false;
        f << "badge.x=" << c.regiaoBadge.x << "\n";
        f << "badge.y=" << c.regiaoBadge.y << "\n";
        f << "badge.largura=" << c.regiaoBadge.largura << "\n";
        f << "badge.altura=" << c.regiaoBadge.altura << "\n";
        f << "letra.x=" << c.regiaoLetra.x << "\n";
        f << "letra.y=" << c.regiaoLetra.y << "\n";
        f << "letra.largura=" << c.regiaoLetra.largura << "\n";
        f << "letra.altura=" << c.regiaoLetra.altura << "\n";
        f << "toleranciaVazio=" << c.toleranciaVazio << "\n";
        f << "toleranciaLado=" << c.toleranciaLado << "\n";
    }

    std::ofstream fr(caminhoRefs(caminhoBase), std::ios::trunc | std::ios::binary);
    if (!fr) return false;
    auto escrever = [&](const std::vector<BYTE>& buf) {
        fr.write(reinterpret_cast<const char*>(buf.data()), (std::streamsize)buf.size());
    };
    escrever(c.refVazioBadge);
    escrever(c.refCompraLetra);
    escrever(c.refVendaLetra);
    return (bool)fr;
}

bool carregarCalibracao(Calibracao& c, const std::string& caminhoBase) {
    std::ifstream f(caminhoBase);
    if (!f) return false;

    std::map<std::string, long long> valores;
    std::string linha;
    while (std::getline(f, linha)) {
        auto pos = linha.find('=');
        if (pos == std::string::npos) continue;
        std::string chave = linha.substr(0, pos);
        long long valor = std::atoll(linha.substr(pos + 1).c_str());
        valores[chave] = valor;
    }
    if (valores.empty()) return false;

    c.regiaoBadge.x = (int)valores["badge.x"];
    c.regiaoBadge.y = (int)valores["badge.y"];
    c.regiaoBadge.largura = (int)valores["badge.largura"];
    c.regiaoBadge.altura = (int)valores["badge.altura"];
    c.regiaoLetra.x = (int)valores["letra.x"];
    c.regiaoLetra.y = (int)valores["letra.y"];
    c.regiaoLetra.largura = (int)valores["letra.largura"];
    c.regiaoLetra.altura = (int)valores["letra.altura"];
    c.toleranciaVazio = valores["toleranciaVazio"];
    c.toleranciaLado = valores["toleranciaLado"];

    size_t bytesBadge = (size_t)c.regiaoBadge.largura * c.regiaoBadge.altura * 4;
    size_t bytesLetra = (size_t)c.regiaoLetra.largura * c.regiaoLetra.altura * 4;
    if (bytesBadge == 0 || bytesLetra == 0) return false;

    std::ifstream fr(caminhoRefs(caminhoBase), std::ios::binary);
    if (!fr) return false;

    auto ler = [&](std::vector<BYTE>& buf, size_t bytes) -> bool {
        buf.resize(bytes);
        fr.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)bytes);
        return (bool)fr;
    };

    if (!ler(c.refVazioBadge, bytesBadge)) return false;
    if (!ler(c.refCompraLetra, bytesLetra)) return false;
    if (!ler(c.refVendaLetra, bytesLetra)) return false;
    return true;
}
