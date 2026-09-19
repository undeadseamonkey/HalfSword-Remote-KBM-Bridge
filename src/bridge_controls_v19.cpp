#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cwchar>
#include <initializer_list>
#include "keybindings_v19.h"

int load_bridge_into_half_sword();

namespace {
HANDLE g_mapping = nullptr;
BridgeSettings *g_settings = nullptr;
HWND g_slider = nullptr, g_value = nullptr, g_status = nullptr, g_alt_option = nullptr;
HWND g_record_button = nullptr, g_record_p1 = nullptr, g_record_p2 = nullptr, g_record_both = nullptr;
HWND g_tab = nullptr, g_key_list = nullptr, g_key_combo = nullptr, g_key_assign = nullptr;
HWND g_key_reset = nullptr, g_key_info = nullptr, g_tooltip = nullptr, g_shift_tip = nullptr;
HWND g_main_controls[32]{};
int g_main_count = 0;
wchar_t g_ini_path[MAX_PATH]{};
ULONGLONG g_start_requested = 0;

bool event_exists(const wchar_t *name) {
    HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, name);
    if (!event) return false;
    CloseHandle(event);
    return true;
}

void refresh_status() {
    if (event_exists(L"Local\\HalfSwordBridge19Running")) {
        const LONG recording = InterlockedCompareExchange(&g_settings->recording_player, 0, 0);
        SetWindowTextW(g_status, recording == 3 ? L"Running. Recording both players' input states."
            : recording == 1 ? L"Running. Recording Player 1's input states."
            : recording == 2 ? L"Running. Recording Player 2's input states."
            : L"Running. The joiner's keyboard and mouse control Player 2.");
        g_start_requested = 0;
    } else if (g_start_requested && GetTickCount64() - g_start_requested < 4000) {
        SetWindowTextW(g_status, L"Starting bridge...");
    } else if (g_start_requested) {
        SetWindowTextW(g_status, L"Start failed. Check HalfSwordBridge19.log in this folder.");
        g_start_requested = 0;
    } else {
        SetWindowTextW(g_status, L"Stopped. Start Half Sword, then click Start.");
    }
}

void update_sensitivity() {
    const int percent = static_cast<int>(SendMessageW(g_slider, TBM_GETPOS, 0, 0));
    InterlockedExchange(&g_settings->sensitivity_per_thousand, percent * 10);
    wchar_t label[90]{};
    swprintf_s(label, L"Joiner mouse sensitivity: %d%%", percent);
    SetWindowTextW(g_value, label);
}

void start_bridge(HWND window) {
    if (event_exists(L"Local\\HalfSwordBridge19Running")) return;
    HANDLE start = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\HalfSwordBridge19Start");
    if (start) {
        SetEvent(start);
        CloseHandle(start);
        g_start_requested = GetTickCount64();
        refresh_status();
        return;
    }
    const int code = load_bridge_into_half_sword();
    if (code != 0) {
        const wchar_t *reason = code == 1 ? L"Start Half Sword before pressing Start."
            : code == 3 ? L"This app supports the standard Half Sword game executable. Check the game install."
            : code == 7 ? L"The game did not load the bridge file. Keep the app and DLL together in the same folder."
            : L"Could not start the bridge. Check the game and folder contents.";
        MessageBoxW(window, reason,
            L"Half Sword joiner controls", MB_OK | MB_ICONINFORMATION);
        return;
    }
    g_start_requested = GetTickCount64();
    refresh_status();
}

void stop_bridge() {
    InterlockedExchange(&g_settings->recording_player, 0);
    HANDLE stop = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\HalfSwordBridge19Stop");
    if (stop) {
        SetEvent(stop);
        CloseHandle(stop);
    }
    g_start_requested = 0;
    SetWindowTextW(g_status, L"Stopping bridge...");
}

void reset_inputs() {
    HANDLE reset = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\HalfSwordBridge19Reset");
    if (reset) { SetEvent(reset); CloseHandle(reset); }
}

void update_record_button() {
    const LONG player = InterlockedCompareExchange(&g_settings->recording_player, 0, 0);
    SetWindowTextW(g_record_button, player ? L"Stop recording" : L"Start recording");
}

void toggle_recording(HWND window) {
    const LONG current = InterlockedCompareExchange(&g_settings->recording_player, 0, 0);
    if (current) InterlockedExchange(&g_settings->recording_player, 0);
    else if (event_exists(L"Local\\HalfSwordBridge19Running")) {
        const int player = SendMessageW(g_record_p1, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1
            : SendMessageW(g_record_p2, BM_GETCHECK, 0, 0) == BST_CHECKED ? 2 : 3;
        InterlockedExchange(&g_settings->recording_player, player);
    } else MessageBoxW(window, L"Start the bridge before recording.",
        L"Half Sword joiner controls", MB_OK | MB_ICONINFORMATION);
    update_record_button();
}

void add_tip(HWND window, HWND control, const wchar_t *description) {
    if (!control || !g_tooltip) return;
    TOOLINFOW tip{};
    tip.cbSize = sizeof(tip);
    tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tip.hwnd = window;
    tip.uId = reinterpret_cast<UINT_PTR>(control);
    tip.lpszText = const_cast<wchar_t *>(description);
    SendMessageW(g_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
}

BOOL CALLBACK move_main_control(HWND control, LPARAM parameter) {
    if (control == g_tab || g_main_count >= 32) return TRUE;
    g_main_controls[g_main_count++] = control;
    RECT box{};
    GetWindowRect(control, &box);
    MapWindowPoints(nullptr, reinterpret_cast<HWND>(parameter), reinterpret_cast<POINT *>(&box), 2);
    SetWindowPos(control, nullptr, box.left, box.top + 35, 0, 0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    return TRUE;
}

void show_tab(int selected) {
    for (int i = 0; i < g_main_count; ++i) ShowWindow(g_main_controls[i], selected ? SW_HIDE : SW_SHOW);
    for (HWND control : {g_key_list, g_key_combo, g_key_assign, g_key_reset, g_key_info})
        if (control) ShowWindow(control, selected ? SW_SHOW : SW_HIDE);
}

void add_choice(const wchar_t *name, LONG token) {
    const LRESULT row = SendMessageW(g_key_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    if (row >= 0) SendMessageW(g_key_combo, CB_SETITEMDATA, static_cast<WPARAM>(row), token);
}

void populate_choices() {
    for (LONG key = 4; key <= 29; ++key) {
        wchar_t label[2]{static_cast<wchar_t>(L'A' + key - 4), 0};
        add_choice(label, key);
    }
    for (LONG key = 30; key <= 39; ++key) {
        wchar_t label[2]{static_cast<wchar_t>(key == 39 ? L'0' : L'1' + key - 30), 0};
        add_choice(label, key);
    }
    const struct { const wchar_t *name; LONG code; } extras[] = {
        {L"Enter",40}, {L"Escape",41}, {L"Backspace",42}, {L"Tab",43}, {L"Space",44},
        {L"Minus",45}, {L"Equals",46}, {L"Left bracket",47}, {L"Right bracket",48},
        {L"Backslash",49}, {L"Semicolon",51}, {L"Apostrophe",52}, {L"Grave",53},
        {L"Comma",54}, {L"Period",55}, {L"Slash",56}, {L"Caps Lock",57},
        {L"Left Ctrl",224}, {L"Left Alt",226}, {L"Left Shift (reserved)",225},
        {L"Right Shift (reserved)",229},
        {L"Right arrow",79}, {L"Left arrow",80}, {L"Down arrow",81}, {L"Up arrow",82},
        {L"Mouse left",257}, {L"Mouse right",258}, {L"Mouse middle",272}
    };
    for (const auto &choice : extras) add_choice(choice.name, choice.code);
    for (int n = 1; n <= 12; ++n) {
        wchar_t label[8]{};
        swprintf_s(label, L"F%d", n);
        add_choice(label, 57 + n);
    }
}

const wchar_t *token_label(LONG token) {
    const int count = static_cast<int>(SendMessageW(g_key_combo, CB_GETCOUNT, 0, 0));
    for (int row = 0; row < count; ++row) {
        if (SendMessageW(g_key_combo, CB_GETITEMDATA, row, 0) != token) continue;
        static wchar_t label[80]{};
        SendMessageW(g_key_combo, CB_GETLBTEXT, row, reinterpret_cast<LPARAM>(label));
        return label;
    }
    return L"Unknown";
}

void refresh_bindings() {
    for (int row = 0; row < kBindingCount; ++row) {
        wchar_t label[80]{};
        wcscpy_s(label, token_label(InterlockedCompareExchange(&g_settings->bindings[row], 0, 0)));
        ListView_SetItemText(g_key_list, row, 1, label);
    }
}

void select_binding() {
    const int selected = ListView_GetNextItem(g_key_list, -1, LVNI_SELECTED);
    if (selected < 0) return;
    const LONG token = InterlockedCompareExchange(&g_settings->bindings[selected], 0, 0);
    const int count = static_cast<int>(SendMessageW(g_key_combo, CB_GETCOUNT, 0, 0));
    for (int row = 0; row < count; ++row) {
        if (SendMessageW(g_key_combo, CB_GETITEMDATA, row, 0) == token) {
            SendMessageW(g_key_combo, CB_SETCURSEL, row, 0);
            return;
        }
    }
}

void save_bindings() {
    for (int row = 0; row < kBindingCount; ++row) {
        wchar_t key[24]{}, value[24]{};
        swprintf_s(key, L"Action%d", row);
        swprintf_s(value, L"%ld", InterlockedCompareExchange(&g_settings->bindings[row], 0, 0));
        WritePrivateProfileStringW(L"JoinerKeybinds", key, value, g_ini_path);
    }
}

void show_shift_warning() {
    if (!g_shift_tip) return;
    RECT box{};
    GetWindowRect(g_key_combo, &box);
    TOOLINFOW tip{};
    tip.cbSize = sizeof(tip);
    tip.hwnd = g_key_combo;
    tip.uId = 1;
    tip.lpszText = const_cast<wchar_t *>(
        L"Shift is reserved for Steam Remote Play controls and can exit or interrupt the joiner's session. Use Caps Lock for sprint or choose another key.");
    SendMessageW(g_shift_tip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tip));
    SendMessageW(g_shift_tip, TTM_TRACKPOSITION, 0, MAKELPARAM(box.left + 25, box.bottom + 3));
    SendMessageW(g_shift_tip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tip));
    SetTimer(GetParent(g_key_combo), 2, 4500, nullptr);
}

void assign_binding(HWND window) {
    if (event_exists(L"Local\\HalfSwordBridge19Running")) {
        MessageBoxW(window, L"Click Stop before changing a keybind, then Start again.",
            L"Joiner keybinds", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const int selected = ListView_GetNextItem(g_key_list, -1, LVNI_SELECTED);
    const LRESULT choice = SendMessageW(g_key_combo, CB_GETCURSEL, 0, 0);
    if (selected < 0 || choice < 0) return;
    const LONG token = static_cast<LONG>(SendMessageW(g_key_combo, CB_GETITEMDATA, choice, 0));
    if (token == 225 || token == 229) { show_shift_warning(); return; }
    for (int row = 0; row < kBindingCount; ++row) {
        if (row != selected && InterlockedCompareExchange(&g_settings->bindings[row], 0, 0) == token) {
            MessageBoxW(window, L"That key is already assigned to another action. Change that action first.",
                L"Joiner keybinds", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    InterlockedExchange(&g_settings->bindings[selected], token);
    save_bindings();
    refresh_bindings();
}

void reset_bindings(HWND window) {
    if (event_exists(L"Local\\HalfSwordBridge19Running")) {
        MessageBoxW(window, L"Click Stop before resetting keybinds.",
            L"Joiner keybinds", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (int row = 0; row < kBindingCount; ++row)
        InterlockedExchange(&g_settings->bindings[row], kDefaultBindings[row]);
    save_bindings();
    refresh_bindings();
    select_binding();
}

bool load_bindings() {
    if (!GetModuleFileNameW(nullptr, g_ini_path, MAX_PATH)) return false;
    wchar_t *slash = wcsrchr(g_ini_path, L'\\');
    if (!slash) return false;
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - g_ini_path), L"HalfSwordJoinerControls.ini");
    LONG loaded[kBindingCount]{};
    bool valid = true;
    for (int row = 0; row < kBindingCount; ++row) {
        wchar_t key[24]{};
        swprintf_s(key, L"Action%d", row);
        loaded[row] = GetPrivateProfileIntW(L"JoinerKeybinds", key, kDefaultBindings[row], g_ini_path);
        if (loaded[row] == 225 || loaded[row] == 229 || loaded[row] < 4 ||
            (loaded[row] > 82 && loaded[row] != 224 && loaded[row] != 226 &&
             loaded[row] != 257 && loaded[row] != 258 && loaded[row] != 272)) valid = false;
        for (int earlier = 0; earlier < row; ++earlier)
            if (loaded[earlier] == loaded[row]) valid = false;
    }
    for (int row = 0; row < kBindingCount; ++row)
        InterlockedExchange(&g_settings->bindings[row], valid ? loaded[row] : kDefaultBindings[row]);
    return true;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE,
                10, 10, 455, 525, window, reinterpret_cast<HMENU>(20), nullptr, nullptr);
            TCITEMW page{};
            page.mask = TCIF_TEXT;
            page.pszText = const_cast<wchar_t *>(L"Controls");
            TabCtrl_InsertItem(g_tab, 0, &page);
            page.pszText = const_cast<wchar_t *>(L"Keybinds");
            TabCtrl_InsertItem(g_tab, 1, &page);
            CreateWindowExW(0, L"STATIC", L"Host controls for Half Sword Remote Play Together",
                WS_CHILD | WS_VISIBLE, 20, 16, 420, 24, window, nullptr, nullptr, nullptr);
            g_status = CreateWindowExW(0, L"STATIC", L"Stopped. Start Half Sword, then click Start.",
                WS_CHILD | WS_VISIBLE, 20, 47, 430, 34, window, nullptr, nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 91, 205, 37, window, reinterpret_cast<HMENU>(1), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                238, 91, 205, 37, window, reinterpret_cast<HMENU>(2), nullptr, nullptr);
            g_value = CreateWindowExW(0, L"STATIC", L"Joiner mouse sensitivity: 25%",
                WS_CHILD | WS_VISIBLE, 20, 151, 330, 24, window, nullptr, nullptr, nullptr);
            g_slider = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ,
                20, 181, 420, 45, window, nullptr, nullptr, nullptr);
            SendMessageW(g_slider, TBM_SETRANGE, TRUE, MAKELPARAM(10, 200));
            const LONG raw = InterlockedCompareExchange(&g_settings->sensitivity_per_thousand, 0, 0);
            SendMessageW(g_slider, TBM_SETPOS, TRUE, raw >= 100 && raw <= 2000 ? raw / 10 : 25);
            update_sensitivity();
            g_alt_option = CreateWindowExW(0, L"BUTTON", L"Use native mouse movement while holding Alt",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 229, 420, 26,
                window, reinterpret_cast<HMENU>(4), nullptr, nullptr);
            SendMessageW(g_alt_option, BM_SETCHECK,
                InterlockedCompareExchange(&g_settings->alt_native_mouse, 0, 0) ? BST_CHECKED : BST_UNCHECKED, 0);
            CreateWindowExW(0, L"BUTTON", L"Reset stuck Player 2 inputs",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 268, 423, 34,
                window, reinterpret_cast<HMENU>(3), nullptr, nullptr);
            CreateWindowExW(0, L"STATIC", L"Record input states for troubleshooting:",
                WS_CHILD | WS_VISIBLE, 20, 312, 423, 23, window, nullptr, nullptr, nullptr);
            g_record_p1 = CreateWindowExW(0, L"BUTTON", L"Player 1 (host)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 20, 340, 205, 25,
                window, reinterpret_cast<HMENU>(6), nullptr, nullptr);
            g_record_p2 = CreateWindowExW(0, L"BUTTON", L"Player 2 (joiner)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 238, 340, 205, 25,
                window, reinterpret_cast<HMENU>(7), nullptr, nullptr);
            g_record_both = CreateWindowExW(0, L"BUTTON", L"Both players (one file)",
                WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 20, 371, 423, 25,
                window, reinterpret_cast<HMENU>(9), nullptr, nullptr);
            SendMessageW(g_record_both, BM_SETCHECK, BST_CHECKED, 0);
            g_record_button = CreateWindowExW(0, L"BUTTON", L"Start recording",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 405, 423, 34,
                window, reinterpret_cast<HMENU>(8), nullptr, nullptr);
            CreateWindowExW(0, L"STATIC", L"Recordings save beside this app as .log files.",
                WS_CHILD | WS_VISIBLE, 20, 448, 423, 23, window, nullptr, nullptr, nullptr);
            EnumChildWindows(window, move_main_control, reinterpret_cast<LPARAM>(window));

            g_key_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                20, 58, 423, 335, window, reinterpret_cast<HMENU>(30), nullptr, nullptr);
            ListView_SetExtendedListViewStyle(g_key_list, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.cx = 270;
            column.pszText = const_cast<wchar_t *>(L"Joiner action");
            ListView_InsertColumn(g_key_list, 0, &column);
            column.cx = 130;
            column.pszText = const_cast<wchar_t *>(L"Remote key");
            ListView_InsertColumn(g_key_list, 1, &column);
            g_key_combo = CreateWindowExW(0, WC_COMBOBOXW, L"",
                WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                20, 405, 245, 250, window, reinterpret_cast<HMENU>(31), nullptr, nullptr);
            populate_choices();
            g_key_assign = CreateWindowExW(0, L"BUTTON", L"Assign key",
                WS_CHILD | BS_PUSHBUTTON, 275, 405, 168, 32,
                window, reinterpret_cast<HMENU>(32), nullptr, nullptr);
            g_key_reset = CreateWindowExW(0, L"BUTTON", L"Restore default keybinds",
                WS_CHILD | BS_PUSHBUTTON, 20, 450, 423, 32,
                window, reinterpret_cast<HMENU>(33), nullptr, nullptr);
            g_key_info = CreateWindowExW(0, L"STATIC",
                L"Choose an action and a key, then Assign. Stop controls before editing.",
                WS_CHILD, 20, 493, 423, 27, window, nullptr, nullptr, nullptr);
            for (int row = 0; row < kBindingCount; ++row) {
                LVITEMW item{};
                item.mask = LVIF_TEXT;
                item.iItem = row;
                item.pszText = const_cast<wchar_t *>(kBindingActions[row]);
                ListView_InsertItem(g_key_list, &item);
            }
            refresh_bindings();
            ListView_SetItemState(g_key_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            select_binding();

            g_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, window, nullptr, nullptr, nullptr);
            SendMessageW(g_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 900);
            SendMessageW(g_tooltip, TTM_SETDELAYTIME, TTDT_RESHOW, 300);
            SendMessageW(g_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 15000);
            SendMessageW(g_tooltip, TTM_SETMAXTIPWIDTH, 0, 360);
            add_tip(window, g_tab, L"Controls starts and stops the joiner bridge. Keybinds changes Player 2's remote keys.");
            add_tip(window, GetDlgItem(window, 1), L"Enable the joiner's keyboard and mouse for Half Sword Player 2. Start the game first.");
            add_tip(window, GetDlgItem(window, 2), L"Disable the bridge and release Player 2 inputs. The app stays open.");
            add_tip(window, g_slider, L"Change how strongly the joiner's mouse movement affects Player 2.");
            add_tip(window, g_alt_option, L"Switch how Player 2's mouse movement is delivered during Classic Alt Thrust. Compare both modes in-game.");
            add_tip(window, GetDlgItem(window, 3), L"Release Player 2 keys and mouse buttons if one stays held unexpectedly.");
            add_tip(window, g_record_p1, L"Record the host's Player 1 game input states for troubleshooting.");
            add_tip(window, g_record_p2, L"Record the joiner's Player 2 game input states for troubleshooting.");
            add_tip(window, g_record_both, L"Record both players' input states in one paired timeline.");
            add_tip(window, g_record_button, L"Start or stop a timestamped input-state log beside this app. It does not record video or audio.");
            add_tip(window, g_key_list, L"Select a joiner action, then choose its remote key below.");
            add_tip(window, g_key_combo, L"Available remote keys and mouse buttons. Shift is reserved by Steam Remote Play.");
            add_tip(window, g_key_assign, L"Save the selected key for this action. Stop the bridge first; duplicate keys are rejected.");
            add_tip(window, g_key_reset, L"Restore the original joiner keybinds, including Caps Lock for sprint.");
            g_shift_tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT, window, nullptr, nullptr, nullptr);
            SendMessageW(g_shift_tip, TTM_SETMAXTIPWIDTH, 0, 340);
            TOOLINFOW shift_tip{};
            shift_tip.cbSize = sizeof(shift_tip);
            shift_tip.uFlags = TTF_TRACK | TTF_ABSOLUTE;
            shift_tip.hwnd = g_key_combo;
            shift_tip.uId = 1;
            shift_tip.lpszText = const_cast<wchar_t *>(
                L"Shift is reserved for Steam Remote Play controls and can interrupt the joiner's session. Choose another key.");
            SendMessageW(g_shift_tip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&shift_tip));
            SetTimer(window, 1, 500, nullptr);
            refresh_status();
            return 0;
        }
        case WM_HSCROLL:
            if (reinterpret_cast<HWND>(lparam) == g_slider) update_sensitivity();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == 1) start_bridge(window);
            if (LOWORD(wparam) == 2) stop_bridge();
            if (LOWORD(wparam) == 3) reset_inputs();
            if (LOWORD(wparam) == 8) toggle_recording(window);
            if (LOWORD(wparam) == 32) assign_binding(window);
            if (LOWORD(wparam) == 33) reset_bindings(window);
            if (LOWORD(wparam) == 31 && HIWORD(wparam) == CBN_SELCHANGE) {
                const LRESULT choice = SendMessageW(g_key_combo, CB_GETCURSEL, 0, 0);
                if (choice >= 0) {
                    const LONG token = static_cast<LONG>(SendMessageW(g_key_combo, CB_GETITEMDATA, choice, 0));
                    if (token == 225 || token == 229) show_shift_warning();
                }
            }
            if (LOWORD(wparam) == 4)
                InterlockedExchange(&g_settings->alt_native_mouse,
                    SendMessageW(g_alt_option, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
            return 0;
        case WM_NOTIFY: {
            const auto *notice = reinterpret_cast<const NMHDR *>(lparam);
            if (notice && notice->hwndFrom == g_tab && notice->code == TCN_SELCHANGE)
                show_tab(TabCtrl_GetCurSel(g_tab));
            if (notice && notice->hwndFrom == g_key_list && notice->code == LVN_ITEMCHANGED)
                select_binding();
            return 0;
        }
        case WM_TIMER:
            if (wparam == 1) refresh_status();
            if (wparam == 2) {
                TOOLINFOW tip{};
                tip.cbSize = sizeof(tip);
                tip.hwnd = g_key_combo;
                tip.uId = 1;
                SendMessageW(g_shift_tip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&tip));
                KillTimer(window, 2);
            }
            return 0;
        case WM_DESTROY:
            InterlockedExchange(&g_settings->recording_player, 0);
            KillTimer(window, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    HANDLE single = CreateMutexW(nullptr, TRUE, L"Local\\HalfSwordBridgeControlApp19Single");
    if (!single || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"The Half Sword joiner controls are already open.",
            L"Half Sword joiner controls", MB_OK | MB_ICONINFORMATION);
        if (single) CloseHandle(single);
        return 1;
    }
    g_mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(BridgeSettings), L"Local\\HalfSwordBridgeSettingsV19");
    if (!g_mapping) { CloseHandle(single); return 2; }
    const bool created = GetLastError() != ERROR_ALREADY_EXISTS;
    g_settings = static_cast<BridgeSettings *>(MapViewOfFile(g_mapping,
        FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(BridgeSettings)));
    if (!g_settings) { CloseHandle(g_mapping); CloseHandle(single); return 3; }
    if (created) {
        InterlockedExchange(&g_settings->sensitivity_per_thousand, 250);
        InterlockedExchange(&g_settings->alt_native_mouse, 1);
        InterlockedExchange(&g_settings->recording_player, 0);
    }
    if (!load_bindings()) { UnmapViewOfFile(g_settings); CloseHandle(g_mapping); CloseHandle(single); return 3; }
    InterlockedExchange(&g_settings->recording_player, 0);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&common);
    WNDCLASSW kind{};
    kind.lpfnWndProc = window_proc;
    kind.hInstance = instance;
    kind.lpszClassName = L"HalfSwordBridgeControlApp19";
    kind.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    kind.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&kind);
    HWND window = CreateWindowExW(0, kind.lpszClassName, L"Half Sword joiner controls",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 475, 575, nullptr, nullptr, instance, nullptr);
    if (window) {
        ShowWindow(window, show);
        UpdateWindow(window);
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    UnmapViewOfFile(g_settings);
    CloseHandle(g_mapping);
    ReleaseMutex(single);
    CloseHandle(single);
    return window ? 0 : 4;
}





