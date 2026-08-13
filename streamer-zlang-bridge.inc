/*
    About: Helpers that create/update dynamic 3D text labels and object
    material texts with a translation for every language loaded in zlang, in
    one call, by wrapping the streamer's *LangText natives
    (UpdateDynamic3DTextLabelLangText, SetDynamicObjectMaterialLangText).

    zlang's Lang: tag is already a small integer slot index, so it is reused
    directly as the streamer "languageid" parameter - no separate mapping
    table is needed.

    This file does not touch player language state at all. You are
    responsible for keeping the streamer's per-player language in sync
    yourself: call Streamer_SetPlayerLanguage(playerid, _:lang) whenever a
    player's language is set (on connect, and on every change via
    Lang_SetPlayerLang / Lang_SetPlayerLangByCode / Lang_SetPlayerLangByName), e.g.:

        new Lang:lang = Lang_SetPlayerLangByCode(playerid, "pl");
        Streamer_SetPlayerLanguage(playerid, _:lang);

    Usage: include this after both streamer.inc and zlang.inc.
*/

#if !defined _streamer_included
    #error Please include streamer.inc before streamer-zlang-bridge
#endif

#if !defined _zlang_included
    #error Please include zlang.inc before streamer-zlang-bridge
#endif

#if defined _streamer_zlang_bridge_included
    #endinput
#endif
#define _streamer_zlang_bridge_included

/*
    Creation / setup helpers

    These pull the translated string for every loaded zlang language out of
    "key" (a zlang var name) and push it into the streamer item as a
    language variant, in addition to the default text.
*/

stock UpdateDynamic3DTextLabelLangTexts(STREAMER_TAG_3D_TEXT_LABEL:label, const key[])
{
    new text[MAX_LANG_MFORMAT_STRING];

    for (new Lang:lang; lang < Lang:MAX_LANGS; lang++)
    {
        if (!Lang_IsValid(lang) || lang == Lang_GetDefaultLang())
        {
            continue;
        }
        Lang_GetText(lang, key, text);
        UpdateDynamic3DTextLabelLangText(label, _:lang, text);
    }
    return 1;
}

stock STREAMER_TAG_3D_TEXT_LABEL:CreateDynamic3DTextLabelLang(const key[], color, Float:x, Float:y, Float:z, Float:drawdistance, attachedplayer = INVALID_PLAYER_ID, attachedvehicle = INVALID_VEHICLE_ID, testlos = 0, worldid = -1, interiorid = -1, playerid = -1, Float:streamdistance = STREAMER_3D_TEXT_LABEL_SD, STREAMER_TAG_AREA:areaid = STREAMER_TAG_AREA:-1, priority = 0)
{
    new text[MAX_LANG_MFORMAT_STRING];
    Lang_GetText(Lang_GetDefaultLang(), key, text);

    new STREAMER_TAG_3D_TEXT_LABEL:label = CreateDynamic3DTextLabel(text, color, x, y, z, drawdistance, attachedplayer, attachedvehicle, testlos, worldid, interiorid, playerid, streamdistance, areaid, priority);
    if (_:label == INVALID_STREAMER_ID)
    {
        return label;
    }

    UpdateDynamic3DTextLabelLangTexts(label, key);
    return label;
}

stock SetDynamicObjectMaterialTextLang(STREAMER_TAG_OBJECT:objectid, materialindex, const key[], materialsize = OBJECT_MATERIAL_SIZE_256x128, const fontface[] = "Arial", fontsize = 24, bold = 1, fontcolor = 0xFFFFFFFF, backcolor = 0, textalignment = 0)
{
    new text[MAX_LANG_MFORMAT_STRING];
    Lang_GetText(Lang_GetDefaultLang(), key, text);

    if (!SetDynamicObjectMaterialText(objectid, materialindex, text, materialsize, fontface, fontsize, bold, fontcolor, backcolor, textalignment))
    {
        return 0;
    }

    for (new Lang:lang; lang < Lang:MAX_LANGS; lang++)
    {
        if (!Lang_IsValid(lang) || lang == Lang_GetDefaultLang())
        {
            continue;
        }
        Lang_GetText(lang, key, text);
        SetDynamicObjectMaterialLangText(objectid, materialindex, _:lang, text);
    }
    return 1;
}
