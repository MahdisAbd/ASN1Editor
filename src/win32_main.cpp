#include "asn1viewer/ber.hpp"
#include "asn1viewer/schema.hpp"

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <fstream>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <utility>
#include <vector>

using namespace asn1viewer;

namespace {

constexpr int kTreeId = 1001;
constexpr int kDetailsId = 1002;
constexpr int kOpenId = 40001;
constexpr int kExitId = 40002;
constexpr int kAppIconId = 101;
constexpr UINT kParseCompleteMessage = WM_APP + 11;

constexpr COLORREF kObjectColor = RGB(19, 92, 125);      // deep teal
constexpr COLORREF kValueColor = RGB(103, 63, 145);      // violet
constexpr COLORREF kErrorColor = RGB(176, 48, 48);       // red
constexpr COLORREF kSelectedBackground = RGB(220, 239, 247);

HWND g_main = nullptr;
HWND g_tree = nullptr;
HWND g_details = nullptr;
HFONT g_font = nullptr;
std::vector<std::shared_ptr<Asn1Node>> g_roots;
std::wstring g_fileName;
std::thread g_decodeWorker;
    std::atomic<std::uint64_t> g_decodeGeneration{0};
bool g_loading = false;
SchemaMatch g_schemaMatch;

struct ParseResult {
    std::uint64_t generation = 0;
    std::wstring path;
    std::vector<std::shared_ptr<Asn1Node>> roots;
    std::string error;
    SchemaMatch match;
};

std::filesystem::path locateSchema(const std::string& filename) {
    std::vector<std::filesystem::path> candidates;
    candidates.push_back(std::filesystem::current_path() / filename);
    candidates.push_back(std::filesystem::current_path() / "specs" / filename);
    wchar_t modulePath[MAX_PATH] = {};
    const auto length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length != 0) {
        const auto executableDirectory = std::filesystem::path(modulePath).parent_path();
        candidates.push_back(executableDirectory / filename);
        candidates.push_back(executableDirectory / "specs" / filename);
        candidates.push_back(executableDirectory.parent_path() / filename);
        candidates.push_back(executableDirectory.parent_path() / "specs" / filename);
        candidates.push_back(executableDirectory.parent_path().parent_path() / filename);
        candidates.push_back(executableDirectory.parent_path().parent_path() / "specs" / filename);
    }
    for (const auto& candidate : candidates) if (std::filesystem::exists(candidate)) return candidate;
    return {};
}

std::wstring wide(const std::string& value) {
    if (value.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}

std::string nodeLabel(const Asn1Node& node) {
    std::string name = !node.fieldName.empty() ? node.fieldName : node.displayName;
    if (name.empty()) name = "Unknown field";
    std::string tag = "[" + std::to_string(node.tagNumber) + "]";
    if (node.tagClass == TagClass::Application) tag = "[APPLICATION " + std::to_string(node.tagNumber) + "]";
    else if (node.tagClass == TagClass::Universal) tag = "[UNIVERSAL " + std::to_string(node.tagNumber) + "]";
    std::string label = name + " " + tag;
    if (!node.schemaType.empty() && node.schemaType != name) label += " : " + node.schemaType;
    if (!node.constructed) {
        std::string value = node.normalizedValue.empty() ? node.decodedValue : node.normalizedValue;
        if (value.empty()) value = "<unknown type>";
        constexpr std::size_t maxValueLength = 64;
        if (value.size() > maxValueLength) value = value.substr(0, maxValueLength - 3) + "...";
        label += " = " + value;
    } else if (!node.children.empty()) {
        label += "  (" + std::to_string(node.children.size()) + " items)";
    }
    return label;
}

std::wstring details(const Asn1Node& node) {
    std::ostringstream out;
    if (g_schemaMatch.matched || !g_schemaMatch.specification.empty()) {
        out << "Specification: " << g_schemaMatch.specification << "\r\n"
            << "Schema match: " << g_schemaMatch.confidence << "\r\n"
            << "Mapping: " << g_schemaMatch.reason << "\r\n\r\n";
    }
    out << "Field: " << (!node.fieldName.empty() ? node.fieldName : node.displayName) << "\r\n"
        << "Tag class: " << tagClassName(node.tagClass) << "\r\n"
        << "Tag number: " << node.tagNumber << "\r\n"
        << "ASN.1 type: " << valueTypeName(node) << "\r\n"
        << "Semantic type: " << (node.schemaType.empty() ? "unresolved" : node.schemaType) << "\r\n"
        << "Underlying type: " << (node.storageType.empty() ? "unresolved" : node.storageType) << "\r\n"
        << "Parent type: " << (node.parentType.empty() ? "unresolved" : node.parentType) << "\r\n"
        << "Constructed: " << (node.constructed ? "yes" : "no") << "\r\n"
        << "Offset: 0x" << std::uppercase << std::hex << node.encodedRange.offset
        << " (" << std::dec << node.encodedRange.offset << ")\r\n"
        << "Encoded length: " << node.encodedRange.length << "\r\n"
        << "Content length: " << node.contentRange.length << "\r\n"
        << "Status: " << statusName(node.status) << "\r\n";
    if (!node.normalizedValue.empty()) out << "Normalized value: " << node.normalizedValue << "\r\n";
    if (!node.normalizer.empty()) out << "Normalizer: " << node.normalizer << "\r\n";
    if (!node.rawValueHex.empty()) out << "Raw content: " << node.rawValueHex << "\r\n";
    if (!node.error.empty()) out << "Error: " << node.error << "\r\n";
    out << "\r\nRaw encoded bytes:\r\n" << node.rawHex << "\r\n";
    return wide(out.str());
}

HTREEITEM insertTreeItem(HTREEITEM parent, const std::shared_ptr<Asn1Node>& node) {
    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    const auto label = wide(nodeLabel(*node));
    insert.item.pszText = const_cast<wchar_t*>(label.c_str());
    insert.item.lParam = reinterpret_cast<LPARAM>(node.get());
    const auto item = TreeView_InsertItem(g_tree, &insert);
    if (node->constructed && !node->children.empty()) {
        TVINSERTSTRUCTW placeholder{};
        placeholder.hParent = item;
        placeholder.hInsertAfter = TVI_LAST;
        placeholder.item.mask = TVIF_TEXT | TVIF_PARAM;
        placeholder.item.pszText = const_cast<wchar_t*>(L"Expand to load children...");
        placeholder.item.lParam = 0;
        TreeView_InsertItem(g_tree, &placeholder);
    }
    return item;
}

void populateTree() {
    TreeView_DeleteAllItems(g_tree);
    for (const auto& root : g_roots) insertTreeItem(TVI_ROOT, root);
}

void expandTreeItem(HTREEITEM item) {
    TVITEMW treeItem{};
    treeItem.mask = TVIF_PARAM;
    treeItem.hItem = item;
    if (!TreeView_GetItem(g_tree, &treeItem) || treeItem.lParam == 0) return;
    const auto* node = reinterpret_cast<const Asn1Node*>(treeItem.lParam);
    const auto firstChild = TreeView_GetChild(g_tree, item);
    if (!firstChild) return;
    TVITEMW placeholder{};
    placeholder.mask = TVIF_PARAM;
    placeholder.hItem = firstChild;
    if (!TreeView_GetItem(g_tree, &placeholder) || placeholder.lParam != 0) return;
    TreeView_DeleteItem(g_tree, firstChild);
    for (const auto& child : node->children) insertTreeItem(item, child);
}

void beginDecode(const std::wstring& path) {
    if (g_decodeWorker.joinable()) g_decodeWorker.join();
    const auto generation = ++g_decodeGeneration;
    g_loading = true;
    EnableMenuItem(GetMenu(g_main), kOpenId, MF_BYCOMMAND | MF_GRAYED);
    TreeView_DeleteAllItems(g_tree);
    SetWindowTextW(g_details, L"Decoding file in background...\r\nThe window remains responsive while the file is parsed.");
    g_decodeWorker = std::thread([path, generation] {
        auto result = std::make_unique<ParseResult>();
        result->generation = generation;
        result->path = path;
        std::ifstream file(path, std::ios::binary);
        if (!file) result->error = "Could not open the selected file.";
        else {
            std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
            BinaryReader reader(std::move(bytes));
            result->roots = BerDecoder(reader).decodeAll();
            result->match = detectSchema(result->roots);
            if (result->match.matched) {
                SchemaRegistry registry;
                const auto schemaPath = locateSchema(result->match.specification);
                std::string schemaError;
                if (!schemaPath.empty() && registry.loadFile(schemaPath.string(), schemaError)) {
                    annotateWithSchema(result->roots, registry, result->match.rootType);
                    applySchemaFormatting(result->roots, reader, registry);
                }
            }
        }
        PostMessageW(g_main, kParseCompleteMessage, static_cast<WPARAM>(generation), reinterpret_cast<LPARAM>(result.release()));
    });
}

void openFile() {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter = L"Binary files\0*.dat;*.bin;*.ber;*.*\0All files\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&dialog)) return;

    beginDecode(path);
}

void layout() {
    RECT rect{};
    GetClientRect(g_main, &rect);
    const int split = (rect.right * 55) / 100;
    MoveWindow(g_tree, 0, 0, split, rect.bottom, TRUE);
    MoveWindow(g_details, split, 0, rect.right - split, rect.bottom, TRUE);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_tree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"", WS_VISIBLE | WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
                                 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTreeId)), GetModuleHandleW(nullptr), nullptr);
        g_details = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Select a node to inspect its decoded value and raw bytes.", WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                                    0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDetailsId)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_details, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
        layout();
        return 0;
    }
    case WM_SIZE: layout(); return 0;
    case kParseCompleteMessage: {
        std::unique_ptr<ParseResult> result(reinterpret_cast<ParseResult*>(lParam));
        if (!result || result->generation != g_decodeGeneration) return 0;
        if (g_decodeWorker.joinable()) g_decodeWorker.join();
        g_loading = false;
        EnableMenuItem(GetMenu(g_main), kOpenId, MF_BYCOMMAND | MF_ENABLED);
        if (!result->error.empty()) {
            MessageBoxW(g_main, wide(result->error).c_str(), L"ASN.1 Viewer", MB_ICONERROR);
            return 0;
        }
        g_roots = std::move(result->roots);
        g_fileName = result->path;
        g_schemaMatch = std::move(result->match);
        populateTree();
        std::wstring title = L"ASN.1 Viewer - " + g_fileName;
        if (!g_schemaMatch.specification.empty()) title += L" [" + wide(g_schemaMatch.specification) + L"]";
        SetWindowTextW(g_main, title.c_str());
        if (!g_roots.empty()) TreeView_SelectItem(g_tree, TreeView_GetRoot(g_tree));
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kOpenId) { openFile(); return 0; }
        if (LOWORD(wParam) == kExitId) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_NOTIFY:
        if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == kTreeId && reinterpret_cast<LPNMHDR>(lParam)->code == TVN_ITEMEXPANDINGW) {
            const auto* change = reinterpret_cast<NMTREEVIEWW*>(lParam);
            if (change->action == TVE_EXPAND) expandTreeItem(change->itemNew.hItem);
            return 0;
        }
        if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == kTreeId && reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW) {
            auto* draw = reinterpret_cast<NMTVCUSTOMDRAW*>(lParam);
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT)) {
                const auto* node = reinterpret_cast<const Asn1Node*>(draw->nmcd.lItemlParam);
                draw->clrText = node && node->status == DecodeStatus::Malformed
                    ? kErrorColor : (node && node->constructed ? kObjectColor : kValueColor);
                draw->clrTextBk = (draw->nmcd.uItemState & CDIS_SELECTED) ? kSelectedBackground : RGB(255, 255, 255);
                return CDRF_DODEFAULT;
            }
        }
        if (reinterpret_cast<LPNMHDR>(lParam)->idFrom == kTreeId && reinterpret_cast<LPNMHDR>(lParam)->code == TVN_SELCHANGEDW) {
            const auto* change = reinterpret_cast<NMTREEVIEWW*>(lParam);
            const auto* node = reinterpret_cast<const Asn1Node*>(change->itemNew.lParam);
            if (node) SetWindowTextW(g_details, details(*node).c_str());
            return 0;
        }
        break;
    case WM_DESTROY:
        if (g_decodeWorker.joinable()) g_decodeWorker.join();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_TREEVIEW_CLASSES};
    InitCommonControlsEx(&common);
    g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const wchar_t className[] = L"ASN1ViewerWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconId));
    windowClass.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconId));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);

    g_main = CreateWindowExW(0, className, L"ASN.1 Viewer", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1200, 760, nullptr, nullptr, instance, nullptr);
    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, kOpenId, L"&Open...\tCtrl+O");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, kExitId, L"E&xit");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
    SetMenu(g_main, menu);
    ShowWindow(g_main, show);
    UpdateWindow(g_main);
    openFile();
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return static_cast<int>(message.wParam);
}
