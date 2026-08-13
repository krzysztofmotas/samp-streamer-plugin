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

stock StreamerZLang_SetPlayerLang(playerid, Lang:lang)
{
    new result = Lang_SetPlayerLang(playerid, lang);
    if (result)
    {
        Streamer_SetPlayerLang(playerid, _:lang);
    }
    return result;
}

#if defined _ALS_Lang_SetPlayerLang
    #undef Lang_SetPlayerLang
#else
    #define _ALS_Lang_SetPlayerLang
#endif
#define Lang_SetPlayerLang StreamerZLang_SetPlayerLang

stock Lang:StreamerZLang_SetPlayerLangByCode(playerid, const code[])
{
    new Lang:lang = Lang_SetPlayerLangByCode(playerid, code);
    if (lang != INVALID_LANG_ID)
    {
        Streamer_SetPlayerLang(playerid, _:lang);
    }
    return lang;
}

#if defined _ALS_Lang_SetPlayerLangByCode
    #undef Lang_SetPlayerLangByCode
#else
    #define _ALS_Lang_SetPlayerLangByCode
#endif
#define Lang_SetPlayerLangByCode StreamerZLang_SetPlayerLangByCode

stock Lang:StreamerZLang_SetPlayerLangByName(playerid, const name[])
{
    new Lang:lang = Lang_SetPlayerLangByName(playerid, name);
    if (lang != INVALID_LANG_ID)
    {
        Streamer_SetPlayerLang(playerid, _:lang);
    }
    return lang;
}

#if defined _ALS_Lang_SetPlayerLangByName
    #undef Lang_SetPlayerLangByName
#else
    #define _ALS_Lang_SetPlayerLangByName
#endif
#define Lang_SetPlayerLangByName StreamerZLang_SetPlayerLangByName


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

stock UpdateDynamic3DTextLabelLangTextf(STREAMER_TAG_3D_TEXT_LABEL:label, color, const key[], {Float, _}:...)
{
    new
        template[MAX_LANG_MFORMAT_STRING],
        text[MAX_LANG_MFORMAT_STRING];

    for (new Lang:lang; lang < Lang:MAX_LANGS; lang++)
    {
        if (!Lang_IsValid(lang))
        {
            continue;
        }

        Lang_GetText(lang, key, template);
        if (numargs() > 3)
        {
            va_format(text, sizeof(text), template, va_start<3>);
        }
        else
        {
            strcopy(text, template);
        }

        if (lang == Lang_GetDefaultLang())
        {
            UpdateDynamic3DTextLabelText(label, color, text);
        }
        else
        {
            UpdateDynamic3DTextLabelLangText(label, _:lang, text);
        }
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
