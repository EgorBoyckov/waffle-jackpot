#pragma once

#include "helpers.h"

// Field layout for the Waffle Jackpot tile.
//
// Phase 4 scope only: a plain password tile (tile image, title, status
// text, password, submit) with no jackpot gate yet -- proving the V2
// registration/serialization/logon path works end to end, per the phase
// plan's own criterion ("вход по паролю через свою плитку работает на
// VM"). Phase 5 adds a CPFT_COMMAND_LINK "PULL!" field and switches the
// password/submit fields to start CPFS_HIDDEN until a jackpot (spec
// §6.1); that's a FieldDescriptors.h change this Phase 4 code doesn't
// attempt to pre-guess.
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
    JFI_SMALL_TEXT,
    JFI_PASSWORD,
    JFI_SUBMIT_BUTTON,
    JFI_NUM_FIELDS,  // keep last -- used as the field count
};

struct FIELD_STATE_PAIR
{
    CREDENTIAL_PROVIDER_FIELD_STATE cpfs;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};

// cpfs controls whether the field shows on the deselected/selected tile;
// cpfis controls things like focus. Separate from the descriptor table
// below because a provider might mix and match these per scenario --
// ours doesn't yet, but keeping them apart matches the sample's shape.
static const FIELD_STATE_PAIR s_rgFieldStatePairs[] = {
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},             // JFI_TILEIMAGE
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},              // JFI_LARGE_TEXT
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},              // JFI_SMALL_TEXT
    {CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_FOCUSED},  // JFI_PASSWORD
    {CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE},     // JFI_SUBMIT_BUTTON
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
    {JFI_PASSWORD, CPFT_PASSWORD_TEXT, L"Password"},
    {JFI_SUBMIT_BUTTON, CPFT_SUBMIT_BUTTON, L"Submit"},
};

static_assert(ARRAYSIZE(s_rgCredProvFieldDescriptors) == JFI_NUM_FIELDS,
              "s_rgCredProvFieldDescriptors must have one entry per JACKPOT_FIELD_ID");
