#pragma once

#include "helpers.h"

// Field layout for the Waffle Jackpot tile.
//
// Phase 5 scope: the jackpot gate is real now. Password and submit start
// CPFS_HIDDEN (spec §6.1: "скрыты (CPFS_HIDDEN) до джекпота") -- these are
// the table's *default* states, used to initialize each JackpotCredential
// instance's own mutable copy (JackpotCredential::_rgFieldState); they
// change at runtime via ICredentialProviderCredentialEvents::SetFieldState
// once the modal automaton reports a jackpot, so this table alone doesn't
// describe live state, only the state a fresh tile starts in.
//
// Modeled on the field-table shape in Microsoft's sample common.h
// (Samples/Win7Samples/security/credentialproviders/samplecredentialprovider/common.h,
// MIT-licensed, Copyright (c) Microsoft Corporation) -- same idea (one
// enum of field indices, one field-state table, one descriptor table),
// values are ours.
enum JACKPOT_FIELD_ID
{
    JFI_TILEIMAGE = 0,
    JFI_LARGE_TEXT,
    JFI_SMALL_TEXT,    // status line: "locked" / "JACKPOT! Enter your credentials"
    JFI_COWARD_TEXT,   // static "Sign-in options -> Coward Mode" hint (spec §6.5)
    JFI_PULL_LINK,     // CPFT_COMMAND_LINK "PULL!"
    JFI_PASSWORD,
    JFI_SUBMIT_BUTTON,
    JFI_NUM_FIELDS,  // keep last -- used as the field count
};

struct FIELD_STATE_PAIR
{
    CREDENTIAL_PROVIDER_FIELD_STATE cpfs;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};

// Default (locked) field states a fresh tile starts in.
static const FIELD_STATE_PAIR s_rgFieldStatePairs[] = {
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},            // JFI_TILEIMAGE
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},            // JFI_LARGE_TEXT
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},            // JFI_SMALL_TEXT
    {CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE},   // JFI_COWARD_TEXT
    {CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE},   // JFI_PULL_LINK
    {CPFS_HIDDEN, CPFIS_NONE},                     // JFI_PASSWORD (shown on jackpot)
    {CPFS_HIDDEN, CPFIS_NONE},                     // JFI_SUBMIT_BUTTON (shown on jackpot)
};

static_assert(ARRAYSIZE(s_rgFieldStatePairs) == JFI_NUM_FIELDS,
              "s_rgFieldStatePairs must have one entry per JACKPOT_FIELD_ID");

// Field index, field type, and label (not the value that appears in the
// field -- e.g. the password field's label is never shown, just used for
// accessibility).
static const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR s_rgCredProvFieldDescriptors[] = {
    {JFI_TILEIMAGE, CPFT_TILE_IMAGE, L"Waffle Jackpot tile image"},
    {JFI_LARGE_TEXT, CPFT_LARGE_TEXT, L"WAFFLE JACKPOT"},
    {JFI_SMALL_TEXT, CPFT_SMALL_TEXT, L"Status"},
    {JFI_COWARD_TEXT, CPFT_SMALL_TEXT, L"Sign-in options"},
    {JFI_PULL_LINK, CPFT_COMMAND_LINK, L"PULL!"},
    {JFI_PASSWORD, CPFT_PASSWORD_TEXT, L"Password"},
    {JFI_SUBMIT_BUTTON, CPFT_SUBMIT_BUTTON, L"Submit"},
};

static_assert(ARRAYSIZE(s_rgCredProvFieldDescriptors) == JFI_NUM_FIELDS,
              "s_rgCredProvFieldDescriptors must have one entry per JACKPOT_FIELD_ID");
