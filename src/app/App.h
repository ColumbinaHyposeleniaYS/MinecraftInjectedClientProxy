#pragma once
// Application state and orchestration. Owns the settings, injection sequence,
// proxy monitor, log buffer, page/chrome layout and all input routing.

#include "app/Lang.h"
#include "app/Theme.h"
#include "app/Ui.h"
#include "core/InjectionSequence.h"
#include "core/LogTail.h"
#include "core/ProxyMonitor.h"
#include "core/Settings.h"

#include <map>

namespace micp {

// ---------------------------------------------------------------- pages

enum PageId { PgOverview = 0, PgInject, PgLogs, PgSettings, PgAbout, PgCount };

struct PageInfo
{
    const wchar_t* label;
    IconKind icon;
};

const PageInfo& PageInfoOf(PageId page);

// ---------------------------------------------------------------- ids

enum {
    ID_TL_CLOSE = 1,
    ID_TL_MIN = 2,
    ID_TL_MAX = 3,

    ID_NAV = 10, // + page index

    ID_BTN_INJECT = 20,
    ID_BTN_CANCEL_INJ = 21,
    ID_BTN_DISCONNECT = 22,
    ID_BTN_RECHECK = 23,
    ID_BTN_OPEN_LOGDIR = 24,
    ID_BTN_OPEN_LOGFILE = 25,
    ID_BTN_CLEAR_LOG = 26,
    ID_BTN_GOTO_INJECT = 27,
    ID_BTN_GOTO_SETTINGS = 28,
    ID_BTN_RESET = 29,
    ID_BTN_GITHUB = 30,
    ID_BTN_ADMIN_RESTART = 31,
    ID_BTN_BROWSE_LOG = 32,

    ID_TGL_FOLLOW = 40,
    ID_TGL_AUTOSWITCH = 41,
    ID_TGL_TOASTDONE = 42,

    ID_SEG_THEME = 50,
    ID_SEG_BACKDROP = 51,
    ID_SEG_LOGFILTER = 52,

    ID_ROW_LOG = 61,

    ID_LANG_ROW = 70,   // settings: language field (opens the picker)
    ID_LANG_ITEM = 71,  // picker rows: ID_LANG_ITEM + langIndex

    ID_PAGE_SB = 90, // overlay scrollbar hit zone (page)
    ID_LOG_SB = 91,  // overlay scrollbar hit zone (logs card)

    ID_EDIT_KEYWORD = 100,
    ID_EDIT_PORT = 101,
};

enum LogFilter { LfAll = 0, LfInfo, LfWarn, LfError, LfCount };

struct LogEntry
{
    std::wstring time;   // extracted HH:MM:SS, may be empty
    std::wstring text;
    int level;           // 0 info, 1 warn, 2 error, 3 debug
};

// ---------------------------------------------------------------- layout

struct BtnDef
{
    int id = 0;
    RectF r;
    const wchar_t* label = L"";
    int style = 1;   // see ui::Button
    bool enabled = true;
    bool danger = false;
};

struct TglDef
{
    int id = 0;
    RectF r;
    bool on = false;
};

struct SegDef
{
    int id = 0;
    RectF r;
    int sel = 0;
    std::vector<std::wstring> items;
};

struct RowDef
{
    int id = 0;
    RectF r;
    std::wstring text;
    IconKind icon = IconKind::Doc;
    bool alert = false;
};

struct Layout
{
    // chrome
    RectF root, titlebar, sidebar, content;
    RectF traffic[3];
    RectF nav[PgCount];

    // shared page metrics
    RectF pageBox;    // content column (centered, max width) - also the scroll viewport
    float pageTop = 0;
    float pageContentH = 0;   // full content height (layout coords)
    float pageMaxScroll = 0;  // max(0, pageContentH - pageBox.h); 0 on the logs page
    RectF pageTrack;          // overlay scrollbar track, right of the column
    RectF pageThumb;          // current thumb pill (valid when pageMaxScroll > 0)

    // overview
    RectF ovWarn, ovStatus, ovInfo, ovHint;
    RectF ovBtnInject;

    // inject
    RectF ijConfig, ijSteps;
    RectF ijRowKw, ijRowPort, ijEditKw, ijEditPort;
    RectF ijHintKw, ijHintPort; // hint lines under each field
    RectF ijStepBullets[4], ijStepTexts[4];
    RectF ijBanner, ijBar;
    RectF ijBtnGo, ijBtnRecheck, ijBtnCancel, ijBtnDisc;

    // logs
    RectF lgSeg, lgTgl, lgBtnClear, lgBtnOpen, lgCard, lgHead, lgBody, lgScroll;
    RectF lgTglLabel;

    // settings
    RectF stAppearance, stBehavior, stLog, stResetCard;
    RectF stSegTheme, stSegBackdrop, stSegThemeLabel, stSegBackdropLabel;
    RectF stTgl[3], stTglLabel[3];
    RectF stRowLog, stBrowseLog, stBtnReset, stIniLabel;
    RectF stLangRow, stLangLabel, stLangValue; // language picker field

    // about
    RectF abIcon, abName, abVersion, abTag, abBtnGh, abBtnDoc, abFoot;

    // registered interactive controls (rebuilt every layout pass)
    std::vector<BtnDef> btns;
    std::vector<TglDef> tgls;
    std::vector<SegDef> segs;
    std::vector<RowDef> rows;
};

// ---------------------------------------------------------------- app

class App
{
public:
    void Init(HWND hwnd);
    void Shutdown();

    // window events (called from WndProc)
    void OnPaint(HDC dc, const RECT& rcPaint);
    void OnSize();
    void OnDpiChanged(int dpi, const RECT* suggested);
    void OnSettingChangeTheme();
    void OnActivate(bool active);
    void OnMouseMove(POINT pt, bool fromNc);
    void OnMouseLeave();
    void OnLButtonDown(POINT pt);
    void OnLButtonUp(POINT pt);
    void OnMouseWheel(POINT pt, int delta);
    void OnTimer(int id);
    void OnEditChanged(int ctrlId);
    void OnEditFocus(int ctrlId, bool focused);
    void OnSeqUpdate(bool finished);
    void OnCommand(int id);
    bool WantsHandCursor() const
    {
        if (m_tlHoverAny)
            return true;
        return m_hoverId != 0 && m_hoverId != ID_PAGE_SB && m_hoverId != ID_LOG_SB;
    }
    int HitTestChrome(POINT pt); // WM_NCHITTEST zones beyond HTCLIENT

    // --- animation ---------------------------------------------------------
    // One eased value per animated property; TickAnimations() moves every
    // value toward its target and reports whether anything is still moving.
    struct AnimVal
    {
        float v = 0.0f;
        float target = 0.0f;
    };
    float HoverT(int id) const;                    // 0..1 hover fade of a control
    float KnobT(int id) const;                     // 0..1 switch knob position
    float SegT(int id, int sel) const;             // segmented thumb position
    bool TickAnimations(float dt);                 // advance; true while busy
    void SetHoverTarget(int id, bool on);          // raise/lower a hover fade
    void PokePageSb();                             // show the page scrollbar
    void PokeLogSb();                              // show the logs scrollbar
    void SnapshotPageContent();                    // capture view for crossfade
    RectF PageThumbRect() const; // live page-scrollbar pill (tracks m_pageScroll)

    // actions
    void SwitchPage(PageId page);
    void StartInject();
    void RecheckTargets();
    void ShowToast(const std::wstring& text, int kind); // 0 info 1 ok 2 warn 3 err
    void SaveNow();
    std::wstring ResolvedDllPath() const;
    bool DllFound() const { return m_dllFound; }

    HWND Hwnd() const { return m_hwnd; }
    const Layout& Lay(); // rebuilds when dirty

private:
    void Relayout();
    void ApplyEditPlacement();
    void ClampPageScroll();
    POINT MapContentPoint(POINT pt) const; // layout coords for page controls
    void ApplyThemeInternal();
    void ApplyBackdrop();
    void ApplyEditFonts();
    void ApplyLang(lang::LangId id); // switch language + rebuild fonts/layout
    RectF LangPopupRect() const;     // open picker geometry (window coords)
    void SyncEditsFromSettings();
    void AppendLogLines(const std::vector<std::wstring>& lines);
    float LogMaxScroll(const Layout& L) const; // logs content height overflow

public:
    bool PassesFilter(const LogEntry& e) const;

private:
    void SegChanged(int segId, int item);
    void ToggleSetting(int id);
    void BrowseLog();
    void ClampLogScroll(const Layout& L);
    int DetectLevel(const std::wstring& line);
    std::wstring DetectTime(const std::wstring& line);

public:
    // --- state (public: the paint pass reads it directly) ---
    HWND m_hwnd = nullptr;
    HWND m_editKw = nullptr;
    HWND m_editPort = nullptr;
    int m_dpi = 96;
    bool m_windowActive = true;
    bool m_backdropActive = false;
    bool m_portInvalid = false;
    int m_editFocused = 0;
    float m_stepProgress = 0.0f;
    InjectionSequence::State m_lastSeqPhase = InjectionSequence::State::Idle;

    SettingsData m_set;
    PageId m_page = PgOverview;

    // hover / press
    int m_hoverId = 0;
    int m_pressId = 0;
    bool m_tlHoverAny = false;
    int m_tlHoverIdx = -1;
    POINT m_lastMouse{ -1, -1 };

    // animation state (public: the paint pass reads it directly)
    std::map<int, AnimVal> m_hoverAnims;  // per control id (incl. traffic lights, seg items)
    std::map<int, AnimVal> m_knobAnims;   // toggle switches (id -> 0..1)
    std::map<int, AnimVal> m_segAnims;    // segmented thumbs (id -> item pos)
    AnimVal m_pageAnim;                   // page transition 0..1
    AnimVal m_toastAnim;                  // toast fade in/out
    AnimVal m_sbAnim;                     // page scrollbar reveal (auto-hide)
    AnimVal m_lgSbAnim;                   // logs scrollbar reveal
    AnimVal m_editFocusAnim[2];           // keyword / port focus rings
    float m_pageScrollTarget = 0;         // eased wheel scrolling
    float m_logScrollTarget = 0;
    ULONGLONG m_sbIdle = 0;               // last page scrollbar activity
    ULONGLONG m_lgSbIdle = 0;             // last logs scrollbar activity

    // injection
    InjectionSequence m_seq;
    int m_stepState[4] = { 0, 0, 0, 0 }; // 0 pending 1 active 2 done 3 error
    std::wstring m_stepDetail[4];
    int m_bannerKind = 0; // 0 none, 1 success, 2 error, 3 cancelled
    std::wstring m_bannerText;
    int m_okCount = 0;

    // monitor
    ProxyMonitor m_mon;
    ProxySnapshot m_snap;

    // logs
    LogTail m_tail;
    std::vector<LogEntry> m_lines;
    std::wstring m_logSource;
    int m_filter = LfAll;
    float m_logScroll = 0;
    bool m_logPinned = true; // stick to bottom while new lines arrive
    bool m_logDragging = false;
    bool m_thumbDragging = false;
    float m_thumbGrab = 0;

    // page scroll (every page except logs, which scrolls internally)
    float m_pageScroll = 0;
    bool m_pageThumbDrag = false;
    float m_pageThumbGrab = 0;

    // language picker popup (settings page; drawn above everything else)
    bool m_langOpen = false;
    int m_langHover = -1;    // hovered row, -1 none
    float m_langScroll = 0;  // px scrolled into the 12-row list

    // toast
    std::wstring m_toastText;
    int m_toastKind = 0;
    ULONGLONG m_toastUntil = 0;

    // misc
    ULONGLONG m_now = 0;
    int m_spin = 0;
    bool m_dirty = false;
    bool m_dllFound = false;
    HBRUSH m_editBrush = nullptr;

    bool m_layoutDirty = true;
    Layout m_lay;
    DWORD m_layoutTick = 0;
};

extern App g_app;

} // namespace micp
