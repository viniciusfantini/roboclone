#include "escrever_texto.h"
#include <uiautomation.h>
#include <wrl/client.h>
#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace {

bool comPronto() {
    static HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE || hr == S_FALSE;
}

} // namespace

bool escreverLinha(HWND janela, const std::string& texto) {
    if (!janela || !IsWindow(janela)) return false;
    if (!comPronto()) return false;

    ComPtr<IUIAutomation> automacao;
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&automacao));
    if (FAILED(hr) || !automacao) {
        std::fprintf(stderr, "[debug] falha ao criar IUIAutomation (hr=0x%08lx)\n", hr);
        return false;
    }

    ComPtr<IUIAutomationElement> raiz;
    hr = automacao->ElementFromHandle(janela, &raiz);
    if (FAILED(hr) || !raiz) return false;

    VARIANT vTrue;
    vTrue.vt = VT_BOOL;
    vTrue.boolVal = VARIANT_TRUE;

    ComPtr<IUIAutomationCondition> condicao;
    hr = automacao->CreatePropertyCondition(UIA_IsValuePatternAvailablePropertyId, vTrue, &condicao);
    if (FAILED(hr) || !condicao) return false;

    ComPtr<IUIAutomationElement> campo;
    hr = raiz->FindFirst(TreeScope_Subtree, condicao.Get(), &campo);
    if (FAILED(hr) || !campo) {
        std::fprintf(stderr, "[debug] nao achei um campo de texto (ValuePattern) na janela escolhida\n");
        return false;
    }

    ComPtr<IUIAutomationValuePattern> valor;
    hr = campo->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valor));
    if (FAILED(hr) || !valor) return false;

    BSTR atual = nullptr;
    valor->get_CurrentValue(&atual);
    std::wstring textoAtual = atual ? atual : L"";
    if (atual) SysFreeString(atual);

    std::wstring linha(texto.begin(), texto.end()); // rotulo e' sempre ASCII (C/V/CC/VV/Z + hora)
    std::wstring novo = textoAtual + linha + L"\r\n";

    BSTR bstrNovo = SysAllocString(novo.c_str());
    hr = valor->SetValue(bstrNovo);
    SysFreeString(bstrNovo);

    if (FAILED(hr)) {
        std::fprintf(stderr, "[debug] SetValue falhou (hr=0x%08lx)\n", hr);
        return false;
    }
    return true;
}
