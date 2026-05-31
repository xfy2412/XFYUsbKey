//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// XFY USB Key — Simplified credential tile fields.
//
#pragma once
#include "helpers.h"

// Simplified field IDs: only what XFY USB Key needs.
enum XFY_FIELD_ID
{
    SFI_TILEIMAGE         = 0,
    SFI_LARGE_TEXT        = 1,
    SFI_PASSWORD          = 2,
    SFI_SUBMIT_BUTTON     = 3,
    SFI_REFRESH_LINK      = 4,
    SFI_NUM_FIELDS        = 5,
};

struct FIELD_STATE_PAIR
{
    CREDENTIAL_PROVIDER_FIELD_STATE cpfs;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};

// Show fields in selected/not-selected tile states.
static const FIELD_STATE_PAIR s_rgFieldStatePairs[] =
{
    { CPFS_DISPLAY_IN_BOTH,            CPFIS_NONE    },    // SFI_TILEIMAGE
    { CPFS_DISPLAY_IN_BOTH,            CPFIS_NONE    },    // SFI_LARGE_TEXT
    { CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_FOCUSED },    // SFI_PASSWORD
    { CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_NONE    },    // SFI_SUBMIT_BUTTON
    { CPFS_DISPLAY_IN_SELECTED_TILE,   CPFIS_NONE    },    // SFI_REFRESH_LINK
};

static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR s_rgCredProvFieldDescriptors[] =
{
    { SFI_TILEIMAGE,     CPFT_TILE_IMAGE,    L"Image",           CPFG_CREDENTIAL_PROVIDER_LOGO },
    { SFI_LARGE_TEXT,    CPFT_LARGE_TEXT,    L"XFY USB Key"                                   },
    { SFI_PASSWORD,      CPFT_PASSWORD_TEXT, L"Password"                                       },
    { SFI_SUBMIT_BUTTON, CPFT_SUBMIT_BUTTON, L"Logon"                                          },
    { SFI_REFRESH_LINK,  CPFT_COMMAND_LINK,  L"Rescan USB"                                     },
};
