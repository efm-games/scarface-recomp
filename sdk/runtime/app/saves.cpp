// The save manager: the game's Xbox saves live in hdd\E\UDATA\<title id>\ beside the executable, written by the game
// itself. This lists them, imports a folder holding original Xbox saves, and opens the folder in Explorer.
#include "app/app.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "core/log.h"

namespace app::saves {

namespace {

namespace fs = std::filesystem;

const ImVec4 kAccent(0.569f, 0.620f, 0.510f, 1.0f);
const ImVec4 kDanger(0.85f, 0.42f, 0.38f, 1.0f);

struct Save {
    fs::path path;
    std::string name;
    std::string when;
    uint64_t bytes = 0;
};

struct List {
    std::vector<Save> saves;
    std::string error;
};

std::string Narrow(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size > 0 ? size_t(size) : 0, '\0');
    if (size > 0) WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring out(size > 0 ? size_t(size) : 0, L'\0');
    if (size > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size);
    return out;
}

std::string Trim(std::string text) {
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ' || text.back() == '\t'))
        text.pop_back();
    size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) start++;
    return text.substr(start);
}

// SaveMeta.xbx and TitleMeta.xbx are UTF-16LE with a BOM, one Key=Value per line
std::string ReadMeta(const fs::path& file, const std::string& key) {
    FILE* f = _wfopen(file.c_str(), L"rb");
    if (!f) return {};
    std::vector<char> bytes;
    char block[512];
    for (size_t n; (n = fread(block, 1, sizeof(block), f)) > 0;) bytes.insert(bytes.end(), block, block + n);
    fclose(f);
    std::string text;
    if (bytes.size() >= 2 && uint8_t(bytes[0]) == 0xFF && uint8_t(bytes[1]) == 0xFE) {
        std::wstring wide((bytes.size() - 2) / 2, L'\0');
        for (size_t i = 0; i < wide.size(); i++)
            wide[i] = wchar_t(uint8_t(bytes[2 + i * 2]) | (uint8_t(bytes[3 + i * 2]) << 8));
        text = Narrow(wide);
    } else {
        text.assign(bytes.begin(), bytes.end());
    }
    // small files: one Key=Value per line, after the BOM
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = Trim(text.substr(at, end - at));
        if (line.rfind(key + "=", 0) == 0) return Trim(line.substr(key.size() + 1));
        at = end + 1;
    }
    return {};
}

std::string TimeText(fs::file_time_type time) {
    using namespace std::chrono;
    auto system = system_clock::now() + duration_cast<system_clock::duration>(time - fs::file_time_type::clock::now());
    std::time_t t = system_clock::to_time_t(system);
    std::tm local{};
    localtime_s(&local, &t);
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M", &local);
    return text;
}

std::string SizeText(uint64_t bytes) {
    char text[32];
    if (bytes >= 1024ull * 1024)
        std::snprintf(text, sizeof(text), "%.1f MB", double(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        std::snprintf(text, sizeof(text), "%.0f KB", double(bytes) / 1024.0);
    else
        std::snprintf(text, sizeof(text), "%llu bytes", (unsigned long long)bytes);
    return text;
}

uint64_t FolderBytes(const fs::path& dir) {
    uint64_t total = 0;
    std::error_code error;
    for (fs::recursive_directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
        if (it->is_regular_file(error)) total += it->file_size(error);
    return total;
}

fs::path InstanceDir() {
    std::wstring name = Instance() > 1 ? L"hdd" + std::to_wstring(Instance()) : L"hdd";
    return ExeDir() / name;
}

bool IsSaveFolder(const fs::path& dir) {
    std::error_code error;
    if (!fs::is_directory(dir, error)) return false;
    return fs::is_regular_file(dir / "SaveMeta.xbx", error) || fs::is_regular_file(dir / "save.dat", error) ||
           fs::is_regular_file(dir / "SaveImage.xbx", error);
}

// a picked folder may be one save, a title folder, or a whole E:\UDATA dump: take every save folder under it
void FindSaves(const fs::path& dir, int depth, std::vector<fs::path>& out) {
    if (depth > 8) return;
    if (IsSaveFolder(dir)) {
        out.push_back(dir);
        return;
    }
    std::error_code error;
    for (fs::directory_iterator it(dir, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_directory(error)) continue;
        std::string name = it->path().filename().string();
        if (name == "$RECYCLE.BIN" || name == "System Volume Information") continue;
        FindSaves(it->path(), depth + 1, out);
    }
}

std::string Stamp() {
    std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &t);
    char text[24];
    std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &local);
    return text;
}

// Xbox save folders are named with twelve hex digits; one renamed by hand is given a fresh id on import
bool IsSaveId(const std::string& name) {
    if (name.size() != 12) return false;
    for (char c : name)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    return true;
}

std::string NewSaveId() {
    static std::atomic<uint32_t> counter{0};
    uint64_t t = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
    t ^= uint64_t(counter.fetch_add(1)) << 40;
    char text[16];
    std::snprintf(text, sizeof(text), "%012llX", (unsigned long long)(t & 0xFFFFFFFFFFFFull));
    return text;
}

fs::path Root() {
    if (!kGame.title_id || !*kGame.title_id) return {};
    return InstanceDir() / "E" / "UDATA" / kGame.title_id;
}

List Enumerate() {
    List list;
    fs::path root = Root();
    if (root.empty()) return list;
    std::error_code error;
    fs::create_directories(root, error);
    if (error) {
        list.error = "Could not open " + Utf8Path(root) + ": " + error.message();
        return list;
    }
    for (fs::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
        std::error_code entry_error;
        if (!it->is_directory(entry_error)) continue;
        if (!IsSaveFolder(it->path())) continue;
        Save save;
        save.path = it->path();
        save.name = ReadMeta(save.path / "SaveMeta.xbx", "Name");
        if (save.name.empty()) save.name = save.path.filename().string();
        fs::path data = save.path / "save.dat";
        if (!fs::exists(data, entry_error)) data = save.path;
        save.when = TimeText(fs::last_write_time(data, entry_error));
        save.bytes = FolderBytes(save.path);
        list.saves.push_back(std::move(save));
    }
    std::sort(list.saves.begin(), list.saves.end(),
              [](const Save& a, const Save& b) { return a.when > b.when; });
    return list;
}

bool PickFolder(std::string& out) {
    // the window thread owns COM in the launcher; the render thread reaches here in the overlay
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    const GUID clsid = {0xdc1c5a9c, 0xe88a, 0x4dde, {0xa5, 0xa1, 0x60, 0xf8, 0x2a, 0x20, 0xae, 0xf7}};
    const GUID iid = {0xd57c7288, 0xd4ad, 0x4768, {0xbe, 0x02, 0x9d, 0x96, 0x95, 0x32, 0xd9, 0x60}};
    IFileOpenDialog* dialog = nullptr;
    bool picked = false;
    if (SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid, reinterpret_cast<void**>(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS);
        dialog->SetTitle(L"Choose the folder holding the Xbox save");
        HWND owner = static_cast<HWND>(Window());
        if (!owner) owner = GetActiveWindow();
        if (dialog->Show(owner) == S_OK) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR wide = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wide)) && wide) {
                    out = Narrow(wide);
                    picked = !out.empty();
                    CoTaskMemFree(wide);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (uninit) CoUninitialize();
    return picked;
}

void OpenFolder(const fs::path& path, bool select) {
    std::wstring args;
    if (select) {
        args = L"explorer.exe /select,\"" + path.wstring() + L"\"";
    } else {
        args = L"explorer.exe \"" + path.wstring() + L"\"";
    }
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, args.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// Copies the picked folder's saves into UDATA\<title id>. An existing save of the same folder name is kept under a
// timestamped backup name instead of being overwritten.
bool Import(const fs::path& source, std::string& status, bool& error) {
    error = false;
    fs::path root = Root();
    if (root.empty()) {
        status = "This game has no save title id set.";
        error = true;
        return false;
    }
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        if (fs::is_regular_file(source, ec) && source.extension() == ".zip") {
            status = "Unpack the .zip first, then choose the folder holding the save.";
        } else {
            status = "That folder could not be read.";
        }
        error = true;
        return false;
    }
    std::vector<fs::path> found;
    FindSaves(source, 0, found);
    if (found.empty()) {
        status = "No Xbox save found in that folder. Choose the folder with SaveMeta.xbx (or save.dat) inside.";
        error = true;
        return false;
    }
    fs::create_directories(root, ec);
    if (ec) {
        status = "Could not create " + Utf8Path(root) + ": " + ec.message();
        error = true;
        return false;
    }
    // the title's own metadata, when the picked folder carries it and this install has none
    for (const wchar_t* meta : {L"TitleMeta.xbx", L"TitleImage.xbx"}) {
        if (fs::is_regular_file(root / meta, ec)) continue;
        std::vector<fs::path> candidates = {source / meta, source / "UDATA" / kGame.title_id / meta};
        if (!source.parent_path().empty()) candidates.push_back(source.parent_path() / meta);
        for (const fs::path& candidate : candidates) {
            if (!fs::is_regular_file(candidate, ec)) continue;
            fs::copy_file(candidate, root / meta, fs::copy_options::overwrite_existing, ec);
            break;
        }
    }

    int copied = 0;
    std::string last_error;
    for (const fs::path& dir : found) {
        std::string name = dir.filename().string();
        if (!IsSaveId(name)) name = NewSaveId();
        fs::path target = root / name;
        fs::path temp = root / (name + ".importing");
        std::error_code same_error;
        if (fs::equivalent(dir, target, same_error)) {
            last_error = "it is already in the save folder";
            continue;
        }
        fs::remove_all(temp, ec);
        fs::copy(dir, temp, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) {
            last_error = ec.message();
            fs::remove_all(temp, ec);
            continue;
        }
        if (fs::exists(target, ec)) {
            fs::path backup = root / (name + ".backup-" + Stamp());
            fs::rename(target, backup, ec);
            if (ec) {
                last_error = ec.message();
                fs::remove_all(temp, ec);
                continue;
            }
            LOGI("Saves: kept the existing %s as %s", Utf8Path(target).c_str(), Utf8Path(backup).c_str());
        }
        fs::rename(temp, target, ec);
        if (ec) {
            last_error = ec.message();
            fs::remove_all(temp, ec);
            continue;
        }
        copied++;
        LOGI("Saves: imported %s as %s", Utf8Path(dir).c_str(), Utf8Path(target).c_str());
    }
    if (copied == 0) {
        status = "The save could not be copied" + (last_error.empty() ? std::string(".") : ": " + last_error);
        error = true;
        return false;
    }
    status = copied == 1 ? "Imported one save." : "Imported " + std::to_string(copied) + " saves.";
    if (copied < int(found.size()))
        status += " " + std::to_string(found.size() - copied) + " could not be copied" +
                  (last_error.empty() ? std::string(".") : ": " + last_error);
    return true;
}

}  // namespace

bool ImportFromFolder(const std::string& path, std::string& status) {
    bool error = false;
    return Import(FromUtf8(path), status, error);
}

bool CommandLine() {
    const wchar_t* line = GetCommandLineW();
    const wchar_t* at = std::wcsstr(line, L"--import-save");
    if (!at) return false;
    bool start = at == line || at[-1] == L' ' || at[-1] == L'\t' || at[-1] == L'"';
    if (!start || (at[13] != L' ' && at[13] != L'\t')) return false;
    at += 13;
    while (*at == L' ' || *at == L'\t') at++;
    std::wstring value;
    if (*at == L'"') {
        for (at++; *at && *at != L'"'; at++) value += *at;
    } else {
        for (; *at && *at != L' ' && *at != L'\t'; at++) value += *at;
    }
    if (value.empty()) return false;
    std::string status;
    bool error = false;
    if (Import(fs::path(value), status, error)) {
        LOGI("Saves: %s", status.c_str());
    } else {
        LOGW("Saves: %s", status.c_str());
        std::wstring message = L"The save could not be imported.\n\n" + Widen(status);
        MessageBoxW(nullptr, message.c_str(), kGame.short_title, MB_OK | MB_ICONWARNING);
    }
    return true;
}

void Ui() {
    if (!kGame.title_id || !*kGame.title_id) {
        ImGui::TextDisabled("This game has no save folder set.");
        return;
    }
    static std::string status;
    static bool status_error = false;
    static fs::path pending_delete;
    static std::string pending_name;

    fs::path root = Root();
    std::error_code ec;
    fs::create_directories(root, ec);

    ImGui::TextWrapped("The game's saves are kept beside it in an Xbox-format folder. PC version saves cannot be "
                       "imported: choose a folder holding an original Xbox save (it has SaveMeta.xbx or save.dat "
                       "inside). Changes show in the game's load menu after it is restarted.");
    ImGui::Spacing();
    if (ImGui::Button("Import an Xbox save folder...", ImVec2(230, 0))) {
        std::string picked;
        if (PickFolder(picked)) {
            status.clear();
            Import(FromUtf8(picked), status, status_error);
            if (status_error) LOGW("Saves: %s", status.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Open save folder", ImVec2(160, 0))) OpenFolder(root, false);
    ImGui::TextDisabled("%s", Utf8Path(root).c_str());
    if (!status.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, status_error ? kDanger : kAccent);
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    List list = Enumerate();
    if (!list.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kDanger);
        ImGui::TextWrapped("%s", list.error.c_str());
        ImGui::PopStyleColor();
        return;
    }
    if (list.saves.empty()) {
        ImGui::TextDisabled("No saves here yet. The game writes one when you save a game.");
        return;
    }

    ImGui::TextDisabled("%d save%s", int(list.saves.size()), list.saves.size() == 1 ? "" : "s");
    bool ask_delete = false;
    ImGui::BeginChild("##saves-list", ImVec2(0, 0), false);
    for (const Save& save : list.saves) {
        ImGui::PushID(save.path.filename().string().c_str());
        ImGui::TextUnformatted(save.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s, %s", save.when.c_str(), SizeText(save.bytes).c_str());
        if (ImGui::Button("Show in Explorer", ImVec2(150, 0))) OpenFolder(save.path, true);
        ImGui::SameLine();
        if (ImGui::Button("Delete", ImVec2(90, 0))) {
            pending_delete = save.path;
            pending_name = save.name;
            ask_delete = true;
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    // the popup lives in the tab's window, not the list's id stack
    if (ask_delete) ImGui::OpenPopup("Delete this save?");
    if (ImGui::BeginPopupModal("Delete this save?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete \"%s\" from this PC?", pending_name.c_str());
        ImGui::TextDisabled("The game must be restarted to forget it.");
        ImGui::Spacing();
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            std::error_code remove_error;
            fs::remove_all(pending_delete, remove_error);
            if (remove_error) {
                status = "Could not delete the save: " + remove_error.message();
                status_error = true;
                LOGW("Saves: %s", status.c_str());
            } else {
                status = "Deleted \"" + pending_name + "\".";
                status_error = false;
                LOGI("Saves: deleted %s", Utf8Path(pending_delete).c_str());
            }
            pending_delete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            pending_delete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace app::saves
