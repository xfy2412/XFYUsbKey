//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// CXFYProvider implements ICredentialProvider, which is the main
// interface that logonUI uses to decide which tiles to display.
// In this sample, we will display one tile that uses each of the nine
// available UI controls.

#include <initguid.h>

#include "CXFYProvider.h"
#include "CXFYCredential.h"
#include "guid.h"

#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

CXFYProvider *CXFYProvider::s_pActiveProvider = nullptr;

CXFYProvider::CXFYProvider():
    _cRef(1),
    _pCredentials(nullptr),
    _dwCredentialCount(0),
    _pCredProviderUserArray(nullptr),
    _pCredProviderEvents(nullptr),
    _upAdviseContext(0),
    _fUsbKeyFound(false),
    _pwzDecryptedPassword(nullptr),
    _pwzDecryptedUsername(nullptr)
{
    DllAddRef();
    s_pActiveProvider = this;
    OutputDebugString(L"[XFY] CXFYProvider created (v1.3)\n");
}

CXFYProvider::~CXFYProvider()
{
    _FreeDecryptedPassword();
    if (s_pActiveProvider == this)
        s_pActiveProvider = nullptr;
    _ReleaseEnumeratedCredentials();
    if (_pCredProviderUserArray != nullptr)
    {
        _pCredProviderUserArray->Release();
        _pCredProviderUserArray = nullptr;
    }
    if (_pCredProviderEvents != nullptr)
    {
        _pCredProviderEvents->Release();
        _pCredProviderEvents = nullptr;
    }

    DllRelease();
}

// SetUsageScenario is the provider's cue that it's going to be asked for tiles
// in a subsequent call.
HRESULT CXFYProvider::SetUsageScenario(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD /*dwFlags*/)
{
    HRESULT hr;

    WCHAR szBuf[128];
    StringCchPrintf(szBuf, ARRAYSIZE(szBuf), L"[XFY] SetUsageScenario: cpus=%d\n", cpus);
    OutputDebugString(szBuf);

    // Decide which scenarios to support here. Returning E_NOTIMPL simply tells the caller
    // that we're not designed for that scenario.
    switch (cpus)
    {
    case CPUS_LOGON:
    case CPUS_UNLOCK_WORKSTATION:
        // The reason why we need _fRecreateEnumeratedCredentials is because ICredentialProviderSetUserArray::SetUserArray() is called after ICredentialProvider::SetUsageScenario(),
        // while we need the ICredentialProviderUserArray during enumeration in ICredentialProvider::GetCredentialCount()
        _cpus = cpus;
        _fRecreateEnumeratedCredentials = true;
        hr = S_OK;
        break;

    case CPUS_CHANGE_PASSWORD:
    case CPUS_CREDUI:
        hr = E_NOTIMPL;
        break;

    default:
        hr = E_INVALIDARG;
        break;
    }

    return hr;
}

// SetSerialization takes the kind of buffer that you would normally return to LogonUI for
// an authentication attempt.  It's the opposite of ICredentialProviderCredential::GetSerialization.
// GetSerialization is implement by a credential and serializes that credential.  Instead,
// SetSerialization takes the serialization and uses it to create a tile.
//
// SetSerialization is called for two main scenarios.  The first scenario is in the credui case
// where it is prepopulating a tile with credentials that the user chose to store in the OS.
// The second situation is in a remote logon case where the remote client may wish to
// prepopulate a tile with a username, or in some cases, completely populate the tile and
// use it to logon without showing any UI.
//
// If you wish to see an example of SetSerialization, please see either the SampleCredentialProvider
// sample or the SampleCredUICredentialProvider sample.  [The logonUI team says, "The original sample that
// this was built on top of didn't have SetSerialization.  And when we decided SetSerialization was
// important enough to have in the sample, it ended up being a non-trivial amount of work to integrate
// it into the main sample.  We felt it was more important to get these samples out to you quickly than to
// hold them in order to do the work to integrate the SetSerialization changes from SampleCredentialProvider
// into this sample.]
HRESULT CXFYProvider::SetSerialization(
    _In_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION const * /*pcpcs*/)
{
    return E_NOTIMPL;
}

// Called by LogonUI to give us a callback for notifying credential changes.
HRESULT CXFYProvider::Advise(
    _In_ ICredentialProviderEvents *pcpe,
    _In_ UINT_PTR upAdviseContext)
{
    OutputDebugString(L"[XFY] Advise called\n");
    if (_pCredProviderEvents != nullptr)
    {
        _pCredProviderEvents->Release();
    }
    _pCredProviderEvents = pcpe;
    _pCredProviderEvents->AddRef();
    _upAdviseContext = upAdviseContext;

    // Only check USB when user clicks "Rescan USB".
    return S_OK;
}

// Called by LogonUI when the ICredentialProviderEvents callback is no longer valid.
HRESULT CXFYProvider::UnAdvise()
{
    OutputDebugString(L"[XFY] UnAdvise called\n");
    _FreeDecryptedPassword();

    if (_pCredProviderEvents != nullptr)
    {
        _pCredProviderEvents->Release();
        _pCredProviderEvents = nullptr;
    }
    return S_OK;
}

// Called by LogonUI to determine the number of fields in your tiles.  This
// does mean that all your tiles must have the same number of fields.
// This number must include both visible and invisible fields. If you want a tile
// to have different fields from the other tiles you enumerate for a given usage
// scenario you must include them all in this count and then hide/show them as desired
// using the field descriptors.
HRESULT CXFYProvider::GetFieldDescriptorCount(
    _Out_ DWORD *pdwCount)
{
    *pdwCount = SFI_NUM_FIELDS;
    return S_OK;
}

// Gets the field descriptor for a particular field.
HRESULT CXFYProvider::GetFieldDescriptorAt(
    DWORD dwIndex,
    _Outptr_result_nullonfailure_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpfd)
{
    HRESULT hr;
    *ppcpfd = nullptr;

    // Verify dwIndex is a valid field.
    if ((dwIndex < SFI_NUM_FIELDS) && ppcpfd)
    {
        hr = FieldDescriptorCoAllocCopy(s_rgCredProvFieldDescriptors[dwIndex], ppcpfd);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Sets pdwCount to the number of tiles that we wish to show at this time.
// Sets pdwDefault to the index of the tile which should be used as the default.
// The default tile is the tile which will be shown in the zoomed view by default. If
// more than one provider specifies a default the last used cred prov gets to pick
// the default. If *pbAutoLogonWithDefault is TRUE, LogonUI will immediately call
// GetSerialization on the credential you've specified as the default and will submit
// that credential for authentication without showing any further UI.
HRESULT CXFYProvider::GetCredentialCount(
    _Out_ DWORD *pdwCount,
    _Out_ DWORD *pdwDefault,
    _Out_ BOOL *pbAutoLogonWithDefault)
{
    *pdwDefault = CREDENTIAL_PROVIDER_NO_DEFAULT;

    // If USB key is present, we can auto-login
    *pbAutoLogonWithDefault = _fUsbKeyFound ? TRUE : FALSE;

    if (_fRecreateEnumeratedCredentials)
    {
        _fRecreateEnumeratedCredentials = false;
        _ReleaseEnumeratedCredentials();
        _CreateEnumeratedCredentials();
    }

    *pdwCount = _dwCredentialCount;

    WCHAR szBuf[256];
    StringCchPrintf(szBuf, ARRAYSIZE(szBuf),
        L"[XFY] GetCredentialCount: %d credentials, autoLogon=%d, usbKey=%d\n",
        _dwCredentialCount, *pbAutoLogonWithDefault, _fUsbKeyFound);
    OutputDebugString(szBuf);

    return S_OK;
}

// Returns the credential at the index specified by dwIndex.
HRESULT CXFYProvider::GetCredentialAt(
    DWORD dwIndex,
    _Outptr_result_nullonfailure_ ICredentialProviderCredential **ppcpc)
{
    HRESULT hr = E_INVALIDARG;
    *ppcpc = nullptr;

    if ((dwIndex < _dwCredentialCount) && _pCredentials && _pCredentials[dwIndex])
    {
        hr = _pCredentials[dwIndex]->QueryInterface(IID_PPV_ARGS(ppcpc));
    }
    return hr;
}

// This function will be called by LogonUI after SetUsageScenario succeeds.
// Sets the User Array with the list of users to be enumerated on the logon screen.
HRESULT CXFYProvider::SetUserArray(_In_ ICredentialProviderUserArray *users)
{
    if (_pCredProviderUserArray)
    {
        _pCredProviderUserArray->Release();
    }
    _pCredProviderUserArray = users;
    _pCredProviderUserArray->AddRef();
    return S_OK;
}

void CXFYProvider::_CreateEnumeratedCredentials()
{
    switch (_cpus)
    {
    case CPUS_LOGON:
    case CPUS_UNLOCK_WORKSTATION:
        {
            _EnumerateCredentials();
            break;
        }
    default:
        break;
    }
}

void CXFYProvider::_ReleaseEnumeratedCredentials()
{
    if (_pCredentials != nullptr)
    {
        for (DWORD i = 0; i < _dwCredentialCount; i++)
        {
            if (_pCredentials[i] != nullptr)
            {
                _pCredentials[i]->Release();
                _pCredentials[i] = nullptr;
            }
        }
        delete[] _pCredentials;
        _pCredentials = nullptr;
    }
    _dwCredentialCount = 0;
}

HRESULT CXFYProvider::_EnumerateCredentials()
{
    HRESULT hr = E_UNEXPECTED;
    if (_pCredProviderUserArray == nullptr)
    {
        return hr;
    }

    DWORD dwUserCount;
    _pCredProviderUserArray->GetCount(&dwUserCount);
    if (dwUserCount == 0)
    {
        return hr;
    }

    OutputDebugString(L"[XFY] Enumerating credentials for all users\n");

    // NOTE: Do NOT call _CheckUsbKey() here! It would trigger CredentialsChanged
    // inside GetCredentialCount (re-entrancy), breaking autoLogon.
    // USB check is only triggered by the user clicking "Rescan USB".

    // Allocate credential array
    _pCredentials = new(std::nothrow) CXFYCredential*[dwUserCount];
    if (_pCredentials == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    ZeroMemory(_pCredentials, sizeof(CXFYCredential*) * dwUserCount);
    _dwCredentialCount = 0;

    // Create one credential per user
    for (DWORD i = 0; i < dwUserCount; i++)
    {
        ICredentialProviderUser *pCredUser;
        hr = _pCredProviderUserArray->GetAt(i, &pCredUser);
        if (FAILED(hr))
        {
            continue;
        }

        _pCredentials[_dwCredentialCount] = new(std::nothrow) CXFYCredential();
        if (_pCredentials[_dwCredentialCount] == nullptr)
        {
            pCredUser->Release();
            continue;
        }

        hr = _pCredentials[_dwCredentialCount]->Initialize(
            _cpus, s_rgCredProvFieldDescriptors, s_rgFieldStatePairs, pCredUser);
        pCredUser->Release();

        if (SUCCEEDED(hr))
        {
            // Pass decrypted credentials from USB key
            if (_pwzDecryptedPassword != nullptr)
            {
                _pCredentials[_dwCredentialCount]->SetPassword(_pwzDecryptedPassword);
            }
            if (_pwzDecryptedUsername != nullptr)
            {
                _pCredentials[_dwCredentialCount]->SetUsername(_pwzDecryptedUsername);
            }
            _dwCredentialCount++;
        }
        else
        {
            _pCredentials[_dwCredentialCount]->Release();
            _pCredentials[_dwCredentialCount] = nullptr;
        }
    }

    WCHAR szBuf[128];
    StringCchPrintf(szBuf, ARRAYSIZE(szBuf), L"[XFY] Enumerated %d credentials\n", _dwCredentialCount);
    OutputDebugString(szBuf);

    return _dwCredentialCount > 0 ? S_OK : E_UNEXPECTED;
}

// ─────────────────────────────────────────────
// USB Key
// ─────────────────────────────────────────────

// Read and decrypt a cred.dat file using DPAPI (LocalMachine scope).
bool CXFYProvider::_ReadAndDecryptKeyFile(_In_ PCWSTR pszKeyPath)
{
    OutputDebugString(L"[XFY] Reading key file...\n");

    HANDLE hFile = CreateFile(pszKeyPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        OutputDebugString(L"[XFY] Cannot open key file\n");
        return false;
    }

    // Read: [encLen(4)] + [entropyLen(4)] + [encData] + [entropy]
    DWORD encLen = 0, entropyLen = 0;
    DWORD dwRead = 0;

    WCHAR dbg[256];
    if (!ReadFile(hFile, &encLen, sizeof(encLen), &dwRead, NULL) || dwRead != sizeof(encLen))
    { StringCchPrintf(dbg, 256, L"[XFY] ReadFile encLen failed (%lu)\n", GetLastError()); OutputDebugString(dbg); CloseHandle(hFile); return false; }
    if (!ReadFile(hFile, &entropyLen, sizeof(entropyLen), &dwRead, NULL) || dwRead != sizeof(entropyLen))
    { StringCchPrintf(dbg, 256, L"[XFY] ReadFile entropyLen failed (%lu)\n", GetLastError()); OutputDebugString(dbg); CloseHandle(hFile); return false; }

    StringCchPrintf(dbg, 256, L"[XFY] Key file: encLen=%lu entropyLen=%lu\n", encLen, entropyLen);
    OutputDebugString(dbg);

    BYTE *encBuf = (BYTE*)CoTaskMemAlloc(encLen);
    BYTE *entropyBuf = (BYTE*)CoTaskMemAlloc(entropyLen);
    if (!encBuf || !entropyBuf)
    {
        CoTaskMemFree(encBuf); CoTaskMemFree(entropyBuf);
        CloseHandle(hFile); return false;
    }

    if (!ReadFile(hFile, encBuf, encLen, &dwRead, NULL) || dwRead != encLen)
    { StringCchPrintf(dbg, 256, L"[XFY] ReadFile encData failed (%lu)\n", GetLastError()); OutputDebugString(dbg); CoTaskMemFree(encBuf); CoTaskMemFree(entropyBuf); CloseHandle(hFile); return false; }
    if (!ReadFile(hFile, entropyBuf, entropyLen, &dwRead, NULL) || dwRead != entropyLen)
    { StringCchPrintf(dbg, 256, L"[XFY] ReadFile entropy failed (%lu)\n", GetLastError()); OutputDebugString(dbg); CoTaskMemFree(encBuf); CoTaskMemFree(entropyBuf); CloseHandle(hFile); return false; }

    CloseHandle(hFile);

    // DPAPI decrypt
    DATA_BLOB dataIn, dataOut, entropyBlob;
    dataIn.pbData = encBuf;
    dataIn.cbData = encLen;
    entropyBlob.pbData = entropyBuf;
    entropyBlob.cbData = entropyLen;

    _FreeDecryptedPassword();
    bool ok = false;

    if (CryptUnprotectData(&dataIn, NULL, &entropyBlob, NULL, NULL, 0, &dataOut))
    {
        // dataOut.pbData is UTF-8: "domain\username\0password\0"
        char *pszData = (char*)dataOut.pbData;
        int nTotal = (int)dataOut.cbData;
        // Find username null terminator, bounded by buffer
        int nUserLen = 0;
        while (nUserLen < nTotal && pszData[nUserLen] != '\0') nUserLen++;
        char *pszUsername = pszData;
        char *pszPassword = (nUserLen + 1 < nTotal) ? pszData + nUserLen + 1 : pszData + nTotal;
        int nPassRemain = nTotal - (nUserLen + 1);
        int nPassLen = 0;
        while (nPassLen < nPassRemain && pszPassword[nPassLen] != '\0') nPassLen++;

        // Log decrypted content (partial, for debugging)
        WCHAR szLog[512];
        StringCchPrintf(szLog, 512, L"[XFY] Decrypted: user='%S' passLen=%d cbData=%d\n", pszUsername, nPassLen, dataOut.cbData);
        OutputDebugString(szLog);

        // Convert username to wide string
        int wideUserLen = MultiByteToWideChar(CP_UTF8, 0, pszUsername, nUserLen, NULL, 0);
        if (wideUserLen > 0)
        {
            _pwzDecryptedUsername = (PWSTR)CoTaskMemAlloc((wideUserLen + 1) * sizeof(WCHAR));
            MultiByteToWideChar(CP_UTF8, 0, pszUsername, nUserLen,
                                _pwzDecryptedUsername, wideUserLen);
            _pwzDecryptedUsername[wideUserLen] = L'\0';
        }

        // Convert password to wide string
        int widePassLen = MultiByteToWideChar(CP_UTF8, 0, pszPassword, nPassLen, NULL, 0);
        if (widePassLen > 0)
        {
            _pwzDecryptedPassword = (PWSTR)CoTaskMemAlloc((widePassLen + 1) * sizeof(WCHAR));
            MultiByteToWideChar(CP_UTF8, 0, pszPassword, nPassLen,
                                _pwzDecryptedPassword, widePassLen);
            _pwzDecryptedPassword[widePassLen] = L'\0';
        }

        // Log password first 2 chars for debugging (safe)
        WCHAR szPassLog[128];
        StringCchPrintf(szPassLog, 128, L"[XFY] Decrypted password: first char='%c' len=%d\n",
            _pwzDecryptedPassword ? _pwzDecryptedPassword[0] : L'?', widePassLen);
        OutputDebugString(szPassLog);

        OutputDebugString(L"[XFY] Key decrypted successfully\n");
        ok = (_pwzDecryptedPassword != nullptr);
        LocalFree(dataOut.pbData);
    }
    else
    {
        StringCchPrintf(dbg, 256, L"[XFY] CryptUnprotectData failed (%lu)\n", GetLastError());
        OutputDebugString(dbg);
    }

    CoTaskMemFree(encBuf);
    CoTaskMemFree(entropyBuf);
    return ok;
}

void CXFYProvider::_FreeDecryptedPassword()
{
    if (_pwzDecryptedPassword != nullptr)
    {
        SecureZeroMemory(_pwzDecryptedPassword, wcslen(_pwzDecryptedPassword) * sizeof(WCHAR));
        CoTaskMemFree(_pwzDecryptedPassword);
        _pwzDecryptedPassword = nullptr;
    }
    if (_pwzDecryptedUsername != nullptr)
    {
        CoTaskMemFree(_pwzDecryptedUsername);
        _pwzDecryptedUsername = nullptr;
    }
}

// Check all drives for \.xfykey\cred.dat (USB, CD/DVD, local disk, etc.)
void CXFYProvider::_CheckUsbKey()
{
    bool fFound = false;
    WCHAR szKeyPath[MAX_PATH];

    // If currently has a decrypted password but no key detected, clear it
    // This handles the transition: key present -> key removed
    if (_pwzDecryptedPassword != nullptr && !_fUsbKeyFound)
    {
        // Still checking, leave password for now
    }

    DWORD dwDrives = GetLogicalDrives();
    for (int i = 0; i < 26; i++)
    {
        if (!(dwDrives & (1 << i)))
            continue;

        WCHAR szRoot[4] = {static_cast<WCHAR>(L'A' + i), L':', L'\\', 0};
        UINT uType = GetDriveType(szRoot);

        if (uType != DRIVE_REMOVABLE && uType != DRIVE_CDROM && uType != DRIVE_FIXED)
            continue;

        StringCchPrintf(szKeyPath, ARRAYSIZE(szKeyPath), L"%c:\\.xfykey\\cred.dat", L'A' + (WCHAR)i);
        if (GetFileAttributes(szKeyPath) != INVALID_FILE_ATTRIBUTES)
        {
            if (!(GetFileAttributes(szKeyPath) & FILE_ATTRIBUTE_DIRECTORY))
            {
                OutputDebugString(szKeyPath);
                OutputDebugString(L"\n");
                fFound = true;
                break;
            }
        }
    }

    bool changed = (fFound != _fUsbKeyFound);

    if (fFound)
    {
        // Try to read and decrypt the key file
        if (_ReadAndDecryptKeyFile(szKeyPath))
        {
            _fUsbKeyFound = true;
        }
        else
        {
            // Decryption failed, still mark as found but no password
            _fUsbKeyFound = true;
            _FreeDecryptedPassword();
        }
    }
    else
    {
        _fUsbKeyFound = false;
        _FreeDecryptedPassword();
    }

    if (changed)
    {
        // Force re-enumeration so credentials get the decrypted password
        _fRecreateEnumeratedCredentials = true;

        OutputDebugString(fFound
            ? L"[XFY] USB key detected!\n"
            : L"[XFY] USB key removed\n");

        if (_pCredProviderEvents != nullptr)
        {
            _pCredProviderEvents->CredentialsChanged(_upAdviseContext);
        }
    }
}

// User clicked "Rescan USB" — check for key and trigger re-enumeration.
void CXFYProvider::_TriggerRefresh()
{
    _CheckUsbKey();  // Will call CredentialsChanged internally if key state changed
}

// Static entry point for the credential to call.
void CXFYProvider::TriggerManualRefresh()
{
    OutputDebugString(L"[XFY] Manual refresh triggered by credential\n");
    if (s_pActiveProvider != nullptr)
    {
        s_pActiveProvider->_TriggerRefresh();
    }
}

void CXFYProvider::LoginFailed()
{
    OutputDebugString(L"[XFY] Login failed - clearing USB key\n");
    if (s_pActiveProvider != nullptr)
    {
        s_pActiveProvider->_FreeDecryptedPassword();
        s_pActiveProvider->_fUsbKeyFound = false;
    }
}

// Boilerplate code to create our provider.
HRESULT CXFY_CreateInstance(_In_ REFIID riid, _Outptr_ void **ppv)
{
    HRESULT hr;
    CXFYProvider *pProvider = new(std::nothrow) CXFYProvider();
    if (pProvider)
    {
        hr = pProvider->QueryInterface(riid, ppv);
        pProvider->Release();
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}
