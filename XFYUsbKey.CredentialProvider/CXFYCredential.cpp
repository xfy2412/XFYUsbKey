//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//

#ifndef WIN32_NO_STATUS
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif
#include <unknwn.h>
#include "CXFYCredential.h"
#include "CXFYProvider.h"
#include "guid.h"

CXFYCredential::CXFYCredential():
    _cRef(1),
    _pCredProvCredentialEvents(nullptr),
    _pszQualifiedUserName(nullptr),
    _fIsLocalUser(false),
    _pwzKeyPassword(nullptr)
{
    DllAddRef();

    ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
    ZeroMemory(_rgFieldStatePairs, sizeof(_rgFieldStatePairs));
    ZeroMemory(_rgFieldStrings, sizeof(_rgFieldStrings));
}

CXFYCredential::~CXFYCredential()
{
    if (_pwzKeyPassword != nullptr)
    {
        SecureZeroMemory(_pwzKeyPassword, wcslen(_pwzKeyPassword) * sizeof(WCHAR));
        CoTaskMemFree(_pwzKeyPassword);
        _pwzKeyPassword = nullptr;
    }
    for (int i = 0; i < ARRAYSIZE(_rgFieldStrings); i++)
    {
        CoTaskMemFree(_rgFieldStrings[i]);
        CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
    }
    CoTaskMemFree(_pszQualifiedUserName);
    DllRelease();
}


// Set the password from the decrypted USB key file (private, never shown in UI).
void CXFYCredential::SetPassword(_In_ PCWSTR pwzPassword)
{
    if (pwzPassword == nullptr)
        return;

    OutputDebugString(L"[XFY] SetPassword called\n");
    if (_pwzKeyPassword != nullptr)
    {
        SecureZeroMemory(_pwzKeyPassword, wcslen(_pwzKeyPassword) * sizeof(WCHAR));
        CoTaskMemFree(_pwzKeyPassword);
    }
    SHStrDupW(pwzPassword, &_pwzKeyPassword);
}

// Set the username (domain\username) for authentication.
void CXFYCredential::SetUsername(_In_ PCWSTR pwzUsername)
{
    if (pwzUsername == nullptr)
        return;

    OutputDebugString(L"[XFY] SetUsername called\n");
    CoTaskMemFree(_pszQualifiedUserName);
    SHStrDupW(pwzUsername, &_pszQualifiedUserName);
    // _fIsLocalUser is set in Initialize, keep as true for V1 tile
}

// Initializes one credential with the field information passed in.
// Set the value of the SFI_LARGE_TEXT field to pwzUsername.
HRESULT CXFYCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                      _In_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR const *rgcpfd,
                                      _In_ FIELD_STATE_PAIR const *rgfsp,
                                      _In_ ICredentialProviderUser * /*pcpUser*/)
{
    HRESULT hr = S_OK;
    _cpus = cpus;
    OutputDebugString(L"[XFY] CXFYCredential::Initialize\n");

    // V1 credential: independent tile, no user association.
    _fIsLocalUser = true;

    // Copy the field descriptors for each field. This is useful if you want to vary the field
    // descriptors based on what Usage scenario the credential was created for.
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(_rgCredProvFieldDescriptors); i++)
    {
        _rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &_rgCredProvFieldDescriptors[i]);
    }

    // Initialize the String value of all the fields.
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"XFY USB Key", &_rgFieldStrings[SFI_LARGE_TEXT]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"", &_rgFieldStrings[SFI_PASSWORD]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Logon", &_rgFieldStrings[SFI_SUBMIT_BUTTON]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Rescan USB Key", &_rgFieldStrings[SFI_REFRESH_LINK]);
    }
    // V1 tile: no user association. Username is set via SetUsername() from USB key data.
    _pszQualifiedUserName = nullptr;

    return hr;
}

// LogonUI calls this in order to give us a callback in case we need to notify it of anything.
HRESULT CXFYCredential::Advise(_In_ ICredentialProviderCredentialEvents *pcpce)
{
    if (_pCredProvCredentialEvents != nullptr)
    {
        _pCredProvCredentialEvents->Release();
    }
    return pcpce->QueryInterface(IID_PPV_ARGS(&_pCredProvCredentialEvents));
}

// LogonUI calls this to tell us to release the callback.
HRESULT CXFYCredential::UnAdvise()
{
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
    }
    _pCredProvCredentialEvents = nullptr;
    return S_OK;
}

// LogonUI calls this function when our tile is selected (zoomed)
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the
// field definitions. But if you want to do something
// more complicated, like change the contents of a field when the tile is
// selected, you would do it here.
HRESULT CXFYCredential::SetSelected(_Out_ BOOL *pbAutoLogon)
{
    // If USB key password is available, signal that auto-logon should proceed.
    if (_pwzKeyPassword != nullptr && _pwzKeyPassword[0] != L'\0')
    {
        OutputDebugString(L"[XFY] SetSelected: autoLogon=TRUE (USB key ready)\n");
        *pbAutoLogon = TRUE;
    }
    else
    {
        OutputDebugString(L"[XFY] SetSelected: autoLogon=FALSE\n");
        *pbAutoLogon = FALSE;
    }
    return S_OK;
}

// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. The most common thing to do here (which we do below)
// is to clear out the password field.
HRESULT CXFYCredential::SetDeselected()
{
    HRESULT hr = S_OK;
    if (_rgFieldStrings[SFI_PASSWORD])
    {
        size_t lenPassword = wcslen(_rgFieldStrings[SFI_PASSWORD]);
        SecureZeroMemory(_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*_rgFieldStrings[SFI_PASSWORD]));

        CoTaskMemFree(_rgFieldStrings[SFI_PASSWORD]);
        hr = SHStrDupW(L"", &_rgFieldStrings[SFI_PASSWORD]);

        if (SUCCEEDED(hr) && _pCredProvCredentialEvents)
        {
            _pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, _rgFieldStrings[SFI_PASSWORD]);
        }
    }

    return hr;
}

// Get info for a particular field of a tile. Called by logonUI to get information
// to display the tile.
HRESULT CXFYCredential::GetFieldState(DWORD dwFieldID,
                                         _Out_ CREDENTIAL_PROVIDER_FIELD_STATE *pcpfs,
                                         _Out_ CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE *pcpfis)
{
    HRESULT hr;

    // Validate our parameters.
    if ((dwFieldID < ARRAYSIZE(_rgFieldStatePairs)))
    {
        *pcpfs = _rgFieldStatePairs[dwFieldID].cpfs;
        *pcpfis = _rgFieldStatePairs[dwFieldID].cpfis;
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

// Sets ppwsz to the string value of the field at the index dwFieldID
HRESULT CXFYCredential::GetStringValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ PWSTR *ppwsz)
{
    HRESULT hr;
    *ppwsz = nullptr;

    // Check to make sure dwFieldID is a legitimate index
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors))
    {
        // Make a copy of the string and return that. The caller
        // is responsible for freeing it.
        hr = SHStrDupW(_rgFieldStrings[dwFieldID], ppwsz);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Get the image to show in the user tile
HRESULT CXFYCredential::GetBitmapValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ HBITMAP *phbmp)
{
    HRESULT hr;
    *phbmp = nullptr;

    if ((SFI_TILEIMAGE == dwFieldID))
    {
        HBITMAP hbmp = LoadBitmap(HINST_THISDLL, MAKEINTRESOURCE(IDB_TILE_IMAGE));
        if (hbmp != nullptr)
        {
            hr = S_OK;
            *phbmp = hbmp;
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Sets pdwAdjacentTo to the index of the field the submit button should be
// adjacent to. We recommend that the submit button is placed next to the last
// field which the user is required to enter information in. Optional fields
// should be below the submit button.
HRESULT CXFYCredential::GetSubmitButtonValue(DWORD dwFieldID, _Out_ DWORD *pdwAdjacentTo)
{
    HRESULT hr;

    if (SFI_SUBMIT_BUTTON == dwFieldID)
    {
        // pdwAdjacentTo is a pointer to the fieldID you want the submit button to
        // appear next to.
        *pdwAdjacentTo = SFI_PASSWORD;
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

// Sets the value of a field which can accept a string as a value.
// This is called on each keystroke when a user types into an edit field
HRESULT CXFYCredential::SetStringValue(DWORD dwFieldID, _In_ PCWSTR pwz)
{
    HRESULT hr;

    // Validate parameters.
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
        (CPFT_EDIT_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft ||
        CPFT_PASSWORD_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        PWSTR *ppwszStored = &_rgFieldStrings[dwFieldID];
        CoTaskMemFree(*ppwszStored);
        hr = SHStrDupW(pwz, ppwszStored);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Stub implementations for unused interface methods.
HRESULT CXFYCredential::GetCheckboxValue(DWORD, _Out_ BOOL*, _Outptr_result_nullonfailure_ PWSTR *ppwszLabel)
{ *ppwszLabel = nullptr; return E_NOTIMPL; }
HRESULT CXFYCredential::SetCheckboxValue(DWORD, BOOL)
{ return E_NOTIMPL; }
HRESULT CXFYCredential::GetComboBoxValueCount(DWORD, _Out_ DWORD*, _Deref_out_range_(<, *) _Out_ DWORD*)
{ return E_NOTIMPL; }
HRESULT CXFYCredential::GetComboBoxValueAt(DWORD, DWORD, _Outptr_result_nullonfailure_ PWSTR *ppwszItem)
{ *ppwszItem = nullptr; return E_NOTIMPL; }
HRESULT CXFYCredential::SetComboBoxSelectedValue(DWORD, DWORD)
{ return E_NOTIMPL; }

// Called when the user clicks "Rescan USB".
HRESULT CXFYCredential::CommandLinkClicked(DWORD dwFieldID)
{
    HRESULT hr = S_OK;

    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
        (CPFT_COMMAND_LINK == _rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        if (dwFieldID == SFI_REFRESH_LINK)
        {
            OutputDebugString(L"[XFY] Refresh USB check triggered by user\n");
            CXFYProvider::TriggerManualRefresh();
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Collect the username and password into a serialized credential for the correct usage scenario
// (logon/unlock is what's demonstrated in this sample).  LogonUI then passes these credentials
// back to the system to log on.
HRESULT CXFYCredential::GetSerialization(_Out_ CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpgsr,
                                            _Out_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpcs,
                                            _Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
                                            _Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
    HRESULT hr = E_UNEXPECTED;
    *pcpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;
    ZeroMemory(pcpcs, sizeof(*pcpcs));
    OutputDebugString(L"[XFY] GetSerialization called\n");
    WCHAR szDbg[256];
    StringCchPrintf(szDbg, 256, L"[XFY] GS: user='%s' isLocal=%d pwzKey=%d\n",
        _pszQualifiedUserName ? _pszQualifiedUserName : L"null",
        _fIsLocalUser, _pwzKeyPassword != nullptr ? 1 : 0);
    OutputDebugString(szDbg);

    // For empty tile, avoid authentication since no user is associated.
    if (_pszQualifiedUserName == nullptr)
    {
        OutputDebugString(L"[XFY] GetSerialization: empty tile - no user, skipping auth\n");
        hr = S_OK;
        *pcpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
    }
    // For local user, the domain and user name can be split from _pszQualifiedUserName (domain\username).
    // CredPackAuthenticationBuffer() cannot be used because it won't work with unlock scenario.
    else if (_fIsLocalUser)
    {
        // Use private password (USB key) if available, otherwise use UI input
        PWSTR pwzPassword = _pwzKeyPassword ? _pwzKeyPassword : _rgFieldStrings[SFI_PASSWORD];
        OutputDebugString(L"[XFY] GetSerialization: local user path\n");

        PWSTR pwzProtectedPassword;
        hr = ProtectIfNecessaryAndCopyPassword(pwzPassword, _cpus, &pwzProtectedPassword);
        if (SUCCEEDED(hr))
        {
            PWSTR pszDomain;
            PWSTR pszUsername;
            hr = SplitDomainAndUsername(_pszQualifiedUserName, &pszDomain, &pszUsername);
            if (SUCCEEDED(hr))
            {
                WCHAR szDbg[256];
                StringCchPrintf(szDbg, 256, L"[XFY] Auth: domain='%s' user='%s'\n", pszDomain, pszUsername);
                OutputDebugString(szDbg);

                KERB_INTERACTIVE_UNLOCK_LOGON kiul;
                hr = KerbInteractiveUnlockLogonInit(pszDomain, pszUsername, pwzProtectedPassword, _cpus, &kiul);
                OutputDebugString(SUCCEEDED(hr) ? L"[XFY] KerbInteractiveUnlockLogonInit OK\n" : L"[XFY] KerbInteractiveUnlockLogonInit FAILED\n");
                if (SUCCEEDED(hr))
                {
                    hr = KerbInteractiveUnlockLogonPack(kiul, &pcpcs->rgbSerialization, &pcpcs->cbSerialization);
                    OutputDebugString(SUCCEEDED(hr) ? L"[XFY] KerbInteractiveUnlockLogonPack OK\n" : L"[XFY] KerbInteractiveUnlockLogonPack FAILED\n");
                    if (SUCCEEDED(hr))
                    {
                        ULONG ulAuthPackage;
                        hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
                        OutputDebugString(SUCCEEDED(hr) ? L"[XFY] RetrieveNegotiateAuthPackage OK\n" : L"[XFY] RetrieveNegotiateAuthPackage FAILED\n");
                        if (SUCCEEDED(hr))
                        {
                            pcpcs->ulAuthenticationPackage = ulAuthPackage;
                            pcpcs->clsidCredentialProvider = CLSID_CXFY;
                            *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
                            OutputDebugString(L"[XFY] GetSerialization: SUCCESS\n");
                        }
                    }
                }
                CoTaskMemFree(pszDomain);
                CoTaskMemFree(pszUsername);
            }
            else
            {
                OutputDebugString(L"[XFY] SplitDomainAndUsername FAILED\n");
            }
            CoTaskMemFree(pwzProtectedPassword);
        }
        else
        {
            OutputDebugString(L"[XFY] ProtectIfNecessaryAndCopyPassword FAILED\n");
        }
    }
    else
    {
        DWORD dwAuthFlags = CRED_PACK_PROTECTED_CREDENTIALS | CRED_PACK_ID_PROVIDER_CREDENTIALS;

        // First get the size of the authentication buffer to allocate
        PWSTR pwzPassword = _pwzKeyPassword ? _pwzKeyPassword : _rgFieldStrings[SFI_PASSWORD];
        if (!CredPackAuthenticationBuffer(dwAuthFlags, _pszQualifiedUserName, const_cast<PWSTR>(pwzPassword), nullptr, &pcpcs->cbSerialization) &&
            (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            pcpcs->rgbSerialization = static_cast<byte *>(CoTaskMemAlloc(pcpcs->cbSerialization));
            if (pcpcs->rgbSerialization != nullptr)
            {
                hr = S_OK;

                // Retrieve the authentication buffer
                if (CredPackAuthenticationBuffer(dwAuthFlags, _pszQualifiedUserName, const_cast<PWSTR>(pwzPassword), pcpcs->rgbSerialization, &pcpcs->cbSerialization))
                {
                    ULONG ulAuthPackage;
                    hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
                    if (SUCCEEDED(hr))
                    {
                        pcpcs->ulAuthenticationPackage = ulAuthPackage;
                        pcpcs->clsidCredentialProvider = CLSID_CXFY;

                        // At this point the credential has created the serialized credential used for logon
                        // By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
                        // that we have all the information we need and it should attempt to submit the
                        // serialized credential.
                        *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
                    }
                }
                else
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    if (SUCCEEDED(hr))
                    {
                        hr = E_FAIL;
                    }
                }

                if (FAILED(hr))
                {
                    CoTaskMemFree(pcpcs->rgbSerialization);
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
            }
        }
    }
    return hr;
}

struct REPORT_RESULT_STATUS_INFO
{
    NTSTATUS ntsStatus;
    NTSTATUS ntsSubstatus;
    PWSTR     pwzMessage;
    CREDENTIAL_PROVIDER_STATUS_ICON cpsi;
};

static const REPORT_RESULT_STATUS_INFO s_rgLogonStatusInfo[] =
{
    { STATUS_LOGON_FAILURE, STATUS_SUCCESS, L"Incorrect password or username.", CPSI_ERROR, },
    { STATUS_ACCOUNT_RESTRICTION, STATUS_ACCOUNT_DISABLED, L"The account is disabled.", CPSI_WARNING },
};

// ReportResult is completely optional.  Its purpose is to allow a credential to customize the string
// and the icon displayed in the case of a logon failure.  For example, we have chosen to
// customize the error shown in the case of bad username/password and in the case of the account
// being disabled.
HRESULT CXFYCredential::ReportResult(NTSTATUS ntsStatus,
                                        NTSTATUS ntsSubstatus,
                                        _Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
                                        _Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    DWORD dwStatusInfo = (DWORD)-1;

    // Look for a match on status and substatus.
    for (DWORD i = 0; i < ARRAYSIZE(s_rgLogonStatusInfo); i++)
    {
        if (s_rgLogonStatusInfo[i].ntsStatus == ntsStatus && s_rgLogonStatusInfo[i].ntsSubstatus == ntsSubstatus)
        {
            dwStatusInfo = i;
            break;
        }
    }

    if ((DWORD)-1 != dwStatusInfo)
    {
        if (SUCCEEDED(SHStrDupW(s_rgLogonStatusInfo[dwStatusInfo].pwzMessage, ppwszOptionalStatusText)))
        {
            *pcpsiOptionalStatusIcon = s_rgLogonStatusInfo[dwStatusInfo].cpsi;
        }
    }

    // If we failed the logon, clear USB key and stop auto-retry.
    if (FAILED(HRESULT_FROM_NT(ntsStatus)))
    {
        CXFYProvider::LoginFailed();
        if (_pwzKeyPassword != nullptr)
        {
            SecureZeroMemory(_pwzKeyPassword, wcslen(_pwzKeyPassword) * sizeof(WCHAR));
            CoTaskMemFree(_pwzKeyPassword);
            _pwzKeyPassword = nullptr;
        }
        if (_pCredProvCredentialEvents)
        {
            _pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, L"");
        }
    }

    // Since nullptr is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
    // this function can't fail.
    return S_OK;
}

// V1 credential - no GetUserSid or GetFieldOptions.
