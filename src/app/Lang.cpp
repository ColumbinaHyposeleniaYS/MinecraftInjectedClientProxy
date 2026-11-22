#include "app/Lang.h"

#include <cwchar>
#include <cwctype>
#include <cstring>

// One pure-ASCII header per language (generated; see tools/gen_lang.py).
// They are only ever included here, so their static arrays exist once.
#include "app/lang/de_DE.h"
#include "app/lang/en_US.h"
#include "app/lang/ja_JP.h"
#include "app/lang/nl_BE.h"
#include "app/lang/nl_NL.h"
#include "app/lang/pt_BR.h"
#include "app/lang/pt_PT.h"
#include "app/lang/ru_RU.h"
#include "app/lang/tr_TR.h"
#include "app/lang/uk_UA.h"
#include "app/lang/zh_CN.h"
#include "app/lang/zh_TW.h"

namespace micp {
namespace lang {
namespace {

// kLangCount tables, each kStrCount long - asserted below.
const wchar_t* const* const kTables[kLangCount] = {
    strings::de_DE, strings::en_US, strings::ja_JP, strings::nl_BE,
    strings::nl_NL, strings::pt_BR, strings::pt_PT, strings::ru_RU,
    strings::tr_TR, strings::uk_UA, strings::zh_CN, strings::zh_TW,
};

const wchar_t* const kCodes[kLangCount] = {
    L"de_DE", L"en_US", L"ja_JP", L"nl_BE", L"nl_NL", L"pt_BR",
    L"pt_PT", L"ru_RU", L"tr_TR", L"uk_UA", L"zh_CN", L"zh_TW",
};

// Endonyms, Minecraft language-menu style. Intentionally NOT translated:
// a user who cannot read the current UI language still finds their own
// language written the way they expect it.
const wchar_t* const kDisplay[kLangCount] = {
    L"Deutsch (Deutschland)",
    L"English (US)",
    L"\u65e5\u672c\u8a9e (\u65e5\u672c)",               // 日本語 (日本)
    L"Nederlands (Belgi\u00eb)",
    L"Nederlands (Nederland)",
    L"Portugu\u00eas (Brasil)",
    L"Portugu\u00eas (Portugal)",
    L"\u0420\u0443\u0441\u0441\u043a\u0438\u0439 (\u0420\u043e\u0441\u0441\u0438\u044f)", // Русский (Россия)
    L"T\u00fcrk\u00e7e (T\u00fcrkiye)",
    L"\u0423\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430 (\u0423\u043a\u0440\u0430\u0457\u043d\u0430)", // Українська (Україна)
    L"\u7b80\u4f53\u4e2d\u6587 (\u4e2d\u56fd)",         // 简体中文 (中国)
    L"\u7e41\u9ad4\u4e2d\u6587 (\u53f0\u7063)",         // 繁體中文 (台灣)
};

LangId g_current = LangId::ZhCn;

} // namespace

LangId Current() { return g_current; }

void SetLang(LangId id)
{
    if (id >= LangId::De && id < LangId::Count)
        g_current = id;
}

const wchar_t* DisplayName(LangId id)
{
    const int i = int(id);
    return (i >= 0 && i < kLangCount) ? kDisplay[i] : kDisplay[int(LangId::ZhCn)];
}

const wchar_t* Code(LangId id)
{
    const int i = int(id);
    return (i >= 0 && i < kLangCount) ? kCodes[i] : kCodes[int(LangId::ZhCn)];
}

const wchar_t* Tr(Str s)
{
    const int i = int(s);
    if (i < 0 || i >= kStrCount)
        return L"";
    const wchar_t* text = kTables[int(g_current)][i];
    return text ? text : kTables[int(LangId::ZhCn)][i];
}

LangId FromLocale(const wchar_t* locale)
{
    if (!locale || !*locale)
        return LangId::ZhCn;
    wchar_t buf[LOCALE_NAME_MAX_LENGTH] = {};
    for (int i = 0; i < LOCALE_NAME_MAX_LENGTH - 1 && locale[i]; ++i)
        buf[i] = wchar_t(towlower(locale[i]));
    const auto has = [&](const wchar_t* prefix)
    { return wcsncmp(buf, prefix, wcslen(prefix)) == 0; };

    // Region-specific forms must be tested before their language prefix
    // (nl-be before nl, pt-br before pt, zh-* before zh).
    if (has(L"de")) return LangId::De;
    if (has(L"en")) return LangId::En;
    if (has(L"ja")) return LangId::Ja;
    if (has(L"nl-be")) return LangId::NlBe;
    if (has(L"nl")) return LangId::NlNl;
    if (has(L"pt-br")) return LangId::PtBr;
    if (has(L"pt")) return LangId::PtPt;
    if (has(L"ru")) return LangId::Ru;
    if (has(L"tr")) return LangId::Tr;
    if (has(L"uk")) return LangId::Uk;
    if (has(L"zh-tw") || has(L"zh-hk") || has(L"zh-mo") || has(L"zh-hant"))
        return LangId::ZhTw;
    if (has(L"zh")) return LangId::ZhCn;
    return LangId::ZhCn; // unmatched locales keep the historical default
}

LangId DetectSystemLang()
{
    wchar_t loc[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(loc, LOCALE_NAME_MAX_LENGTH) > 0)
        return FromLocale(loc);
    return LangId::ZhCn;
}

const wchar_t* CjkFontFace()
{
    switch (g_current)
    {
    case LangId::ZhTw:
        return L"Microsoft JhengHei UI";
    case LangId::Ja:
        return L"Yu Gothic UI";
    default:
        return L"Microsoft YaHei UI";
    }
}

bool UsesCjkFace()
{
    return g_current == LangId::ZhCn || g_current == LangId::ZhTw
           || g_current == LangId::Ja;
}

// Every table must line up with the Str enum exactly; a mismatch is a
// compile error rather than a runtime crash in a shipped build.
#define MICP_TABLE_LEN(a) (sizeof(a) / sizeof((a)[0]))
static_assert(MICP_TABLE_LEN(strings::de_DE) == kStrCount, "de_DE table size mismatch");
static_assert(MICP_TABLE_LEN(strings::en_US) == kStrCount, "en_US table size mismatch");
static_assert(MICP_TABLE_LEN(strings::ja_JP) == kStrCount, "ja_JP table size mismatch");
static_assert(MICP_TABLE_LEN(strings::nl_BE) == kStrCount, "nl_BE table size mismatch");
static_assert(MICP_TABLE_LEN(strings::nl_NL) == kStrCount, "nl_NL table size mismatch");
static_assert(MICP_TABLE_LEN(strings::pt_BR) == kStrCount, "pt_BR table size mismatch");
static_assert(MICP_TABLE_LEN(strings::pt_PT) == kStrCount, "pt_PT table size mismatch");
static_assert(MICP_TABLE_LEN(strings::ru_RU) == kStrCount, "ru_RU table size mismatch");
static_assert(MICP_TABLE_LEN(strings::tr_TR) == kStrCount, "tr_TR table size mismatch");
static_assert(MICP_TABLE_LEN(strings::uk_UA) == kStrCount, "uk_UA table size mismatch");
static_assert(MICP_TABLE_LEN(strings::zh_CN) == kStrCount, "zh_CN table size mismatch");
static_assert(MICP_TABLE_LEN(strings::zh_TW) == kStrCount, "zh_TW table size mismatch");
#undef MICP_TABLE_LEN

} // namespace lang
} // namespace micp
