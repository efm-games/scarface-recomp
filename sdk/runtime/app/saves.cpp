// The save manager: the game's Xbox saves live in hdd\E\UDATA\<title id>\ beside the executable, written by the game
// itself. This lists them, imports original Xbox saves, converts PC/PS2 saves, and opens the folder in Explorer.
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
#include <map>
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

// PC and PS2 saves share the game's named-variable format with Xbox saves. The values are fixed-width strings
// in 32-byte slots; the byte after each name is the current value width. Xbox files split the slot table around
// their signature blocks, so the target offsets are not the same as the source offsets.
namespace convert {

const uint8_t kMagic[4] = {0xB0, 0x24, 0x1C, 0xD7};
const size_t kFirstCount = 68;
const size_t kSlotStride = 32;
const size_t kSlotValue = 31;
const size_t kPcSlots = 0x1776;
const size_t kPs2FirstSlots = 0x1776;
const size_t kPs2SecondSlots = 0x1FFA;
const size_t kXboxFirstSlots = 0x1776;
const size_t kXboxSecondSlots = 0x200A;
const size_t kNoBase = size_t(-1);

// The four bytes at 0x0F identify one of the game's three fixed profiles inside save.dat. The game compares them
// with the profile folder, so copying a template from another slot makes the converted save look damaged. These are
// the identities in the three original Xbox profiles and their matching folder ids.
const size_t kSlotIdentity = 0x0F;
const uint8_t kSlotIds[3][4] = {
    {0x0E, 0x14, 0x10, 0x38},  // profile 1, folder ...C0
    {0x13, 0x03, 0x2E, 0x10},  // profile 2, folder ...C1
    {0x13, 0x03, 0x38, 0x2B},  // profile 3, folder ...C2
};
const char* kSlotFolderIds[3] = {"267AFF5B38C0", "267AFF5B38C1", "267AFF5B38C2"};

struct SignatureRange {
    size_t begin;
    size_t end;
    size_t signature;
};
const SignatureRange kSignatures[] = {
    {0x0000, 0x1FEC, 0x1FEC},
    {0x2000, 0x3FEC, 0x3FEC},
    {0x4000, 0x4601, 0x4601},
};

class Sha1 {
public:
    Sha1() { Reset(); }

    void Update(const uint8_t* data, size_t size) {
        bits_ += uint64_t(size) * 8;
        while (size) {
            size_t n = std::min(size, sizeof(block_) - used_);
            std::memcpy(block_ + used_, data, n);
            used_ += n;
            data += n;
            size -= n;
            if (used_ == sizeof(block_)) {
                Process();
                used_ = 0;
            }
        }
    }

    void Final(uint8_t out[20]) {
        uint64_t bits = bits_;
        uint8_t one = 0x80, zero = 0;
        Update(&one, 1);
        while (used_ != 56) Update(&zero, 1);
        uint8_t length[8];
        for (int i = 0; i < 8; i++) length[i] = uint8_t(bits >> ((7 - i) * 8));
        Update(length, sizeof(length));
        for (int i = 0; i < 5; i++) {
            out[i * 4 + 0] = uint8_t(h_[i] >> 24);
            out[i * 4 + 1] = uint8_t(h_[i] >> 16);
            out[i * 4 + 2] = uint8_t(h_[i] >> 8);
            out[i * 4 + 3] = uint8_t(h_[i]);
        }
    }

private:
    static uint32_t Rotate(uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); }

    void Reset() {
        h_[0] = 0x67452301u;
        h_[1] = 0xEFCDAB89u;
        h_[2] = 0x98BADCFEu;
        h_[3] = 0x10325476u;
        h_[4] = 0xC3D2E1F0u;
        bits_ = 0;
        used_ = 0;
    }

    void Process() {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t(block_[i * 4]) << 24) | (uint32_t(block_[i * 4 + 1]) << 16) |
                   (uint32_t(block_[i * 4 + 2]) << 8) | uint32_t(block_[i * 4 + 3]);
        for (int i = 16; i < 80; i++)
            w[i] = Rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            uint32_t next = Rotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = Rotate(b, 30);
            b = a;
            a = next;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
    }

    uint32_t h_[5];
    uint64_t bits_ = 0;
    uint8_t block_[64]{};
    size_t used_ = 0;
};

// The save signature key is HMAC-SHA1 with a sixteen-byte zero key.
void HmacSha1Zero(const uint8_t* data, size_t size, uint8_t out[20]) {
    uint8_t ipad[64], opad[64], inner[20];
    std::memset(ipad, 0x36, sizeof(ipad));
    std::memset(opad, 0x5C, sizeof(opad));
    Sha1 first;
    first.Update(ipad, sizeof(ipad));
    first.Update(data, size);
    first.Final(inner);
    Sha1 second;
    second.Update(opad, sizeof(opad));
    second.Update(inner, sizeof(inner));
    second.Final(out);
}

uint32_t Le32(const std::vector<uint8_t>& data, size_t at) {
    return uint32_t(data[at]) | (uint32_t(data[at + 1]) << 8) | (uint32_t(data[at + 2]) << 16) |
           (uint32_t(data[at + 3]) << 24);
}

size_t SharedBase(const std::vector<uint8_t>& data) {
    if (data.size() < 0x40) return kNoBase;
    if (Le32(data, 0) == data.size() && std::memcmp(data.data() + 4, kMagic, sizeof(kMagic)) == 0) return 4;
    if (std::memcmp(data.data(), kMagic, sizeof(kMagic)) == 0) return 0;
    return kNoBase;
}

struct Record {
    std::string name;
    uint8_t size = 0;
};

enum class Kind { Pc, Ps2, Xbox };

bool Parse(const std::vector<uint8_t>& data, size_t base, std::vector<Record>& records, std::string& error) {
    if (base == kNoBase || data.size() < base + 0x38) {
        error = "not a save file";
        return false;
    }
    uint32_t count = Le32(data, base + 0x34);
    if (!count || count > 1000 || data.size() < base + 0x38 + size_t(count) * 17) {
        error = "the save's variable table is damaged";
        return false;
    }
    records.clear();
    records.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        size_t at = base + 0x38 + size_t(i) * 17;
        size_t length = 0;
        while (length < 16 && data[at + length]) length++;
        if (!length || length == 16 || data[at + 16] > kSlotValue) {
            error = "the save's variable table is damaged";
            return false;
        }
        std::string name(reinterpret_cast<const char*>(data.data() + at), length);
        for (char c : name)
            if (uint8_t(c) < 0x20 || uint8_t(c) > 0x7E) {
                error = "the save's variable table is damaged";
                return false;
            }
        records.push_back({std::move(name), data[at + 16]});
    }
    return true;
}

size_t SlotOffset(Kind kind, size_t index) {
    if (kind == Kind::Pc) return kPcSlots + index * kSlotStride;
    if (index < kFirstCount) {
        size_t base = kind == Kind::Ps2 ? kPs2FirstSlots : kXboxFirstSlots;
        return base + index * kSlotStride;
    }
    size_t base = kind == Kind::Ps2 ? kPs2SecondSlots : kXboxSecondSlots;
    return base + (index - kFirstCount) * kSlotStride;
}

bool ValueAt(const std::vector<uint8_t>& data, size_t base, Kind kind, size_t index, const Record& record,
             std::vector<uint8_t>& value, std::string& error) {
    size_t at = base + SlotOffset(kind, index);
    if (record.size > kSlotValue || at > data.size() || data.size() - at < record.size) {
        error = "a save value lies outside the file";
        return false;
    }
    value.assign(data.begin() + at, data.begin() + at + record.size);
    return true;
}

bool ReadBinary(const fs::path& path, std::vector<uint8_t>& out) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    out.resize(size_t(size));
    size_t read = fread(out.data(), 1, out.size(), f);
    fclose(f);
    out.resize(read);
    return read == size_t(size);
}

bool WriteBinary(const fs::path& path, const std::vector<uint8_t>& data, std::error_code& error) {
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) {
        error = std::make_error_code(std::errc::permission_denied);
        return false;
    }
    bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
    if (fclose(f) != 0) ok = false;
    if (!ok) error = std::make_error_code(std::errc::io_error);
    return ok;
}

std::vector<uint8_t> MetaBytes(int slot) {
    // The game checks this label against its fixed profile slots. A friendlier name such as "Converted Save" makes it
    // reject the profile as damaged, so generated saves use the same label as the original profile.
    std::string text = "Name=Profile  " + std::to_string(slot) + "\r\n";
    std::vector<uint8_t> out = {0xFF, 0xFE};
    for (char c : text) {
        out.push_back(uint8_t(c));
        out.push_back(0);
    }
    return out;
}

std::vector<uint8_t> ImageBytes() {
    const uint8_t header[] = {
        'X', 'P', 'R', '0', 0x00, 0x10, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00,
        0, 0, 0, 0, 0, 0, 0, 0, 0x29, 0x0C, 0x61, 0x06, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    std::vector<uint8_t> out(4096, 0xAD);
    std::memcpy(out.data(), header, sizeof(header));
    return out;
}

void Resign(std::vector<uint8_t>& data) {
    for (const SignatureRange& range : kSignatures) {
        if (range.end > data.size() || range.signature + 20 > data.size()) continue;
        uint8_t digest[20];
        HmacSha1Zero(data.data() + range.begin, range.end - range.begin, digest);
        std::memcpy(data.data() + range.signature, digest, sizeof(digest));
    }
}

bool LooksLikeSource(const fs::path& path) {
    std::error_code error;
    if (!fs::is_regular_file(path, error) || fs::file_size(path, error) > 4u * 1024u * 1024u) return false;
    std::vector<uint8_t> data;
    if (!ReadBinary(path, data) || SharedBase(data) != 0 || data.size() < 0x38) return false;
    uint32_t count = Le32(data, 0x34);
    return count == 188 || count == 170;
}

void FindSourceFiles(const fs::path& dir, int depth, std::vector<fs::path>& out) {
    if (depth > 6) return;
    std::error_code error;
    for (fs::directory_iterator it(dir, error), end; !error && it != end; it.increment(error)) {
        if (it->is_directory(error))
            FindSourceFiles(it->path(), depth + 1, out);
        else if (it->is_regular_file(error) && LooksLikeSource(it->path()))
            out.push_back(it->path());
    }
}

// The game always opens these three folder ids. Do not derive them from the template: an imported save can have an
// arbitrary id, and changing its last digit would create a folder the game never looks at.
std::string SlotId(int slot, std::string& error) {
    if (slot < 1 || slot > 3) {
        error = "choose Xbox profile 1, 2 or 3";
        return {};
    }
    return kSlotFolderIds[slot - 1];
}

bool One(const fs::path& source, const fs::path& template_save, int slot, const fs::path& target, std::string& label,
         std::string& error) {
    std::vector<uint8_t> source_data, template_data;
    if (!ReadBinary(source, source_data) || !ReadBinary(template_save, template_data)) {
        error = "a save file could not be read";
        return false;
    }
    size_t source_base = SharedBase(source_data), template_base = SharedBase(template_data);
    std::vector<Record> source_records, template_records;
    if (!Parse(source_data, source_base, source_records, error) ||
        !Parse(template_data, template_base, template_records, error))
        return false;
    if (source_base != 0 || template_base != 4 || template_records.size() != 138) {
        error = "choose a PC or PS2 save and keep an Xbox profile as the template";
        return false;
    }
    Kind kind;
    if (source_records.size() == 188) {
        kind = Kind::Pc;
        label = "PC";
    } else if (source_records.size() == 170) {
        kind = Kind::Ps2;
        label = "PS2";
    } else {
        error = "unsupported save format";
        return false;
    }

    std::map<std::string, std::vector<uint8_t>> values;
    for (size_t i = 0; i < source_records.size(); i++) {
        std::vector<uint8_t> value;
        if (!ValueAt(source_data, source_base, kind, i, source_records[i], value, error)) return false;
        values[source_records[i].name] = std::move(value);
    }

    std::vector<uint8_t> out = template_data;
    for (size_t i = 0; i < template_records.size(); i++) {
        auto found = values.find(template_records[i].name);
        if (found == values.end()) continue;  // a value the Xbox profile has and the source does not: keep it
        size_t at = template_base + 0x38 + i * 17;
        size_t slot = template_base + SlotOffset(Kind::Xbox, i);
        size_t limit = std::min(found->second.size(), kSlotValue);
        if (slot < 0x1FEC && slot + limit > 0x1FEC) limit = 0x1FEC - slot;  // the last slot meets a signature
        out[at + 16] = uint8_t(limit);
        if (limit) std::memcpy(out.data() + slot, found->second.data(), limit);
    }
    if (slot >= 1 && slot <= 3 && out.size() >= kSlotIdentity + 4)
        std::memcpy(out.data() + kSlotIdentity, kSlotIds[slot - 1], sizeof(kSlotIds[0]));
    Resign(out);

    fs::path final = target;
    fs::path root = final.parent_path();
    fs::path temp = root / (final.filename().string() + ".converting");
    fs::path backup = root / (final.filename().string() + ".backup-" + Stamp());
    std::error_code ec;
    fs::remove_all(temp, ec);
    fs::create_directories(temp, ec);
    if (ec) {
        error = ec.message();
        fs::remove_all(temp, ec);
        return false;
    }
    if (!WriteBinary(temp / "save.dat", out, ec) || !WriteBinary(temp / "SaveMeta.xbx", MetaBytes(slot), ec) ||
        !WriteBinary(temp / "SaveImage.xbx", ImageBytes(), ec)) {
        error = ec.message();
        fs::remove_all(temp, ec);
        return false;
    }
    if (fs::exists(final, ec)) {
        fs::rename(final, backup, ec);
        if (ec) {
            error = ec.message();
            fs::remove_all(temp, ec);
            return false;
        }
    }
    fs::rename(temp, final, ec);
    if (ec) {
        std::error_code restore_error;
        if (fs::exists(backup, restore_error)) fs::rename(backup, final, restore_error);
        error = ec.message();
        fs::remove_all(temp, restore_error);
        return false;
    }
    LOGI("Saves: converted %s to %s", Utf8Path(source).c_str(), Utf8Path(final).c_str());
    return true;
}

}  // namespace convert

bool Convert(const fs::path& source, int slot, std::string& status, bool& error) {
    error = false;
    fs::path root = Root();
    if (root.empty()) {
        status = "This game has no save title id set.";
        error = true;
        return false;
    }
    std::vector<fs::path> candidates;
    std::error_code ec;
    if (fs::is_regular_file(source, ec)) {
        if (convert::LooksLikeSource(source)) candidates.push_back(source);
    } else if (fs::is_directory(source, ec)) {
        convert::FindSourceFiles(source, 0, candidates);
    }
    if (candidates.empty()) {
        status = "No PC or PS2 save was found there. Choose the folder holding the save file.";
        error = true;
        return false;
    }
    List saves = Enumerate();
    if (saves.saves.empty()) {
        status = "Create and save one Xbox profile in the game first; it supplies the Xbox save template.";
        error = true;
        return false;
    }
    fs::path template_save = saves.saves.front().path / "save.dat";
    if (!fs::is_regular_file(template_save, ec)) {
        status = "The Xbox save template could not be read.";
        error = true;
        return false;
    }

    int chosen = slot;
    fs::path target;
    if (slot == 0) {  // first free Xbox profile, leaving existing profiles alone
        for (int s = 1; s <= 3; s++) {
            std::string slot_error;
            std::string id = convert::SlotId(s, slot_error);
            if (id.empty()) {
                status = slot_error;
                error = true;
                return false;
            }
            if (!fs::exists(root / id, ec)) {
                target = root / id;
                chosen = s;
                break;
            }
        }
        if (target.empty()) {
            status = "All three Xbox profile slots are in use. Choose the profile to replace.";
            error = true;
            return false;
        }
    } else {
        std::string slot_error;
        std::string id = convert::SlotId(slot, slot_error);
        if (id.empty()) {
            status = slot_error;
            error = true;
            return false;
        }
        target = root / id;
    }

    std::string last_error, label;
    if (!convert::One(candidates.front(), template_save, chosen, target, label, last_error)) {
        status = "The save could not be converted" +
                 (last_error.empty() ? std::string(".") : ": " + last_error);
        error = true;
        return false;
    }
    status = "Converted the " + label + " save into Xbox profile " + std::to_string(chosen) + ".";
    if (candidates.size() > 1)
        status += " " + std::to_string(candidates.size() - 1) + " other save files were ignored.";
    return true;
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
        dialog->SetTitle(L"Choose the folder holding the save to import or convert");
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
bool Import(const fs::path& source, int slot, std::string& status, bool& error) {
    error = false;
    fs::path root = Root();
    if (root.empty()) {
        status = "This game has no save title id set.";
        error = true;
        return false;
    }
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        if (fs::is_regular_file(source, ec) && source.extension() != ".zip") {
            // Convert() leaves the specific rejection reason in status; do not replace it with a generic message.
            return Convert(source, slot, status, error);
        }
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
    if (found.empty()) return Convert(source, slot, status, error);
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
    return Import(FromUtf8(path), 0, status, error);
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
    int slot = 0;
    const wchar_t* slot_at = std::wcsstr(at, L"--save-slot");
    if (slot_at) {
        slot_at += 11;
        while (*slot_at == L' ' || *slot_at == L'\t') slot_at++;
        if (*slot_at >= L'1' && *slot_at <= L'3') slot = int(*slot_at - L'0');
    }
    std::string status;
    bool error = false;
    if (Import(fs::path(value), slot, status, error)) {
        LOGI("Saves: %s", status.c_str());
    } else {
        LOGW("Saves: %s", status.c_str());
        MessageBoxW(nullptr, (L"The save could not be imported or converted.\n\n" + Widen(status)).c_str(),
                    kGame.short_title, MB_OK | MB_ICONWARNING);
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
    static int convert_slot = 0;
    static fs::path pending_delete;
    static std::string pending_name;

    fs::path root = Root();
    std::error_code ec;
    fs::create_directories(root, ec);

    ImGui::TextWrapped("The game reads and writes Xbox-format saves in the folder below. You can import an original "
                       "Xbox save, or convert a PC or PS2 save into a free or chosen Xbox profile. Conversion writes "
                       "an Xbox-format profile; it does not move or change the source save. Keep one Xbox profile in "
                       "the game first: it supplies the template. Choosing a profile that already has a save backs "
                       "that save up before replacing it. Changes show in the load menu after a restart.");
    ImGui::Spacing();
    ImGui::Combo("Convert into", &convert_slot,
                 "Free Xbox profile\0Xbox profile 1\0Xbox profile 2\0Xbox profile 3");
    if (ImGui::Button("Import or convert a save folder...", ImVec2(250, 0))) {
        std::string picked;
        if (PickFolder(picked)) {
            status.clear();
            Import(FromUtf8(picked), convert_slot, status, status_error);
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
