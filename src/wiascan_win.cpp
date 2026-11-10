// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 (siehe LICENSE)
#include "wiascan_win.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

#include <cwchar>

namespace WiaScan {

namespace {

// WIA-Konstanten (wiadef.h / WIA Automation Layer)
constexpr long ScannerDeviceType = 1;
constexpr long ColorIntent = 1;
constexpr long GrayscaleIntent = 2;
constexpr long MaximizeQuality = 131072;
const wchar_t *const FormatPNG = L"{B96B3CAF-0728-11D3-9D7B-0000F81EF32E}";

constexpr HRESULT WIA_ERROR_GENERAL_ERROR = HRESULT(0x80210001L);
constexpr HRESULT WIA_ERROR_PAPER_JAM = HRESULT(0x80210002L);
constexpr HRESULT WIA_ERROR_PAPER_EMPTY = HRESULT(0x80210003L);
constexpr HRESULT WIA_ERROR_OFFLINE = HRESULT(0x80210005L);
constexpr HRESULT WIA_ERROR_BUSY = HRESULT(0x80210006L);
constexpr HRESULT WIA_ERROR_WARMING_UP = HRESULT(0x80210007L);
constexpr HRESULT WIA_ERROR_DEVICE_COMMUNICATION = HRESULT(0x8021000AL);
constexpr HRESULT WIA_ERROR_DEVICE_LOCKED = HRESULT(0x8021000DL);
constexpr HRESULT WIA_ERROR_COVER_OPEN = HRESULT(0x80210016L);
constexpr HRESULT WIA_ERROR_LAMP_OFF = HRESULT(0x80210017L);
constexpr HRESULT WIA_S_NO_DEVICE_AVAILABLE = HRESULT(0x80210015L);

void setError(wchar_t *buf, int len, const wchar_t *text)
{
    if (!buf || len <= 0) return;
    int i = 0;
    for (; text && text[i] && i < len - 1; ++i) buf[i] = text[i];
    buf[i] = 0;
}

void setHResult(wchar_t *buf, int len, const wchar_t *prefix, HRESULT hr, const EXCEPINFO *ex)
{
    const wchar_t *known = nullptr;
    switch (ex && ex->scode ? HRESULT(ex->scode) : hr) {
    case WIA_ERROR_PAPER_JAM: known = L"Papierstau im Scanner."; break;
    case WIA_ERROR_PAPER_EMPTY: known = L"Im Einzug liegt kein Papier."; break;
    case WIA_ERROR_OFFLINE: known = L"Der Scanner ist nicht eingeschaltet oder nicht verbunden."; break;
    case WIA_ERROR_BUSY: known = L"Der Scanner ist gerade beschäftigt."; break;
    case WIA_ERROR_WARMING_UP: known = L"Der Scanner wärmt sich noch auf – bitte gleich noch einmal versuchen."; break;
    case WIA_ERROR_DEVICE_COMMUNICATION: known = L"Keine Verbindung zum Scanner (Kabel, Netzwerk, Treiber?)."; break;
    case WIA_ERROR_DEVICE_LOCKED: known = L"Der Scanner ist gesperrt (Transportsicherung?)."; break;
    case WIA_ERROR_COVER_OPEN: known = L"Der Scannerdeckel ist offen."; break;
    case WIA_ERROR_LAMP_OFF: known = L"Die Scannerlampe ist aus."; break;
    case WIA_ERROR_GENERAL_ERROR: known = L"Allgemeiner Scannerfehler."; break;
    default: break;
    }
    wchar_t text[512];
    if (known) {
        setError(buf, len, known);
        return;
    }
    if (ex && ex->bstrDescription && ex->bstrDescription[0])
        swprintf(text, 512, L"%ls: %.400ls", prefix, ex->bstrDescription);
    else
        swprintf(text, 512, L"%ls (Fehlercode 0x%08lX)", prefix, static_cast<unsigned long>(ex && ex->scode ? ex->scode : hr));
    setError(buf, len, text);
}

struct ComInit {
    HRESULT hr;
    ComInit() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComInit()
    {
        if (SUCCEEDED(hr)) CoUninitialize();   // S_OK oder S_FALSE – beides ausgleichen
    }
};

struct Dispatch {
    IDispatch *p = nullptr;
    ~Dispatch()
    {
        if (p) p->Release();
    }
};

HRESULT invoke(IDispatch *d, const wchar_t *name, WORD flags, VARIANT *args, UINT nargs, VARIANT *result, EXCEPINFO *ex)
{
    DISPID id = 0;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    HRESULT hr = d->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    DISPPARAMS dp{};
    dp.rgvarg = args;
    dp.cArgs = nargs;
    UINT argErr = 0;
    hr = d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, result, ex, &argErr);
    if (hr == DISP_E_EXCEPTION && ex && ex->pfnDeferredFillIn) ex->pfnDeferredFillIn(ex);
    return hr;
}

void clearException(EXCEPINFO &ex)
{
    if (ex.bstrSource) SysFreeString(ex.bstrSource);
    if (ex.bstrDescription) SysFreeString(ex.bstrDescription);
    if (ex.bstrHelpFile) SysFreeString(ex.bstrHelpFile);
    ex = EXCEPINFO{};
}

} // namespace

bool available()
{
    ComInit com;
    CLSID clsid;
    return SUCCEEDED(CLSIDFromProgID(L"WIA.CommonDialog", &clsid));
}

Result acquire(const wchar_t *path, bool colorIntent, wchar_t *error, int errorLen)
{
    setError(error, errorLen, L"");
    ComInit com;
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(L"WIA.CommonDialog", &clsid))) {
        setError(error, errorLen, L"Die Windows-Bilderfassung (WIA) ist auf diesem System nicht verfügbar.");
        return Result::NotAvailable;
    }
    Dispatch dlg;
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_IDispatch, reinterpret_cast<void **>(&dlg.p));
    if (FAILED(hr) || !dlg.p) {
        setHResult(error, errorLen, L"Der Windows-Scandialog kann nicht geöffnet werden", hr, nullptr);
        return Result::NotAvailable;
    }

    // ShowAcquireImage(DeviceType, Intent, Bias, FormatID, AlwaysSelectDevice, UseCommonUI, CancelError)
    // – Argumente in umgekehrter Reihenfolge
    VARIANT args[7];
    for (VARIANT &v : args) VariantInit(&v);
    args[6].vt = VT_I4;   args[6].lVal = ScannerDeviceType;
    args[5].vt = VT_I4;   args[5].lVal = colorIntent ? ColorIntent : GrayscaleIntent;
    args[4].vt = VT_I4;   args[4].lVal = MaximizeQuality;
    args[3].vt = VT_BSTR; args[3].bstrVal = SysAllocString(FormatPNG);
    args[2].vt = VT_BOOL; args[2].boolVal = VARIANT_FALSE;   // nur bei mehreren Scannern auswählen
    args[1].vt = VT_BOOL; args[1].boolVal = VARIANT_TRUE;    // Windows-Dialog mit Vorschau
    args[0].vt = VT_BOOL; args[0].boolVal = VARIANT_FALSE;   // Abbrechen → leeres Ergebnis statt Fehler
    VARIANT res;
    VariantInit(&res);
    EXCEPINFO ex{};
    hr = invoke(dlg.p, L"ShowAcquireImage", DISPATCH_METHOD, args, 7, &res, &ex);
    VariantClear(&args[3]);
    if (FAILED(hr)) {
        const HRESULT code = (hr == DISP_E_EXCEPTION && ex.scode) ? HRESULT(ex.scode) : hr;
        Result r = Result::Failed;
        if (code == WIA_S_NO_DEVICE_AVAILABLE) {
            setError(error, errorLen, L"Es wurde kein Scanner gefunden. Ist er eingeschaltet, verbunden und der "
                                      L"WIA-Treiber des Herstellers installiert?");
            r = Result::NoDevice;
        } else {
            setHResult(error, errorLen, L"Scannen nicht möglich", hr, &ex);
        }
        clearException(ex);
        VariantClear(&res);
        return r;
    }
    clearException(ex);
    if (res.vt != VT_DISPATCH || !res.pdispVal) {
        VariantClear(&res);
        return Result::Cancelled;
    }
    // ImageFile.SaveFile(Filename) – schlägt fehl, wenn die Datei schon existiert
    DeleteFileW(path);
    VARIANT file;
    VariantInit(&file);
    file.vt = VT_BSTR;
    file.bstrVal = SysAllocString(path);
    hr = invoke(res.pdispVal, L"SaveFile", DISPATCH_METHOD, &file, 1, nullptr, &ex);
    VariantClear(&file);
    VariantClear(&res);
    if (FAILED(hr)) {
        setHResult(error, errorLen, L"Das gescannte Bild kann nicht gespeichert werden", hr, &ex);
        clearException(ex);
        return Result::Failed;
    }
    clearException(ex);
    return Result::Ok;
}

} // namespace WiaScan

#endif // _WIN32
