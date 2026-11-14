// Hooks for default.xbe. A strong definition replaces the recompiled function at that address.
#include "app/app.h"

#include "cpu/x86_ops.h"
#include "core/log.h"

// The startup state machine sets up a playlist of the five publisher logos through sub_00028940 and only advances to
// the menu after the manager finishes it. The manager finishes in phase 5, which restores the script update mask the
// setup saved and lets the boot state machine resume; suppressing the update instead strands the boot, and emptying
// the playlist walks past the end of the table. Skipping is therefore a forced jump to phase 5 on the first update,
// so the original phase 5 block performs the real completion handshake.
constexpr uint32_t kLogoTable = 0x44E968;

X86_EXTERN_FUNC(__imp__sub_00028940);
X86_EXTERN_FUNC(__imp__sub_00028B10);

namespace {

// Set once the playlist of the current boot has been advanced, so the phase is not forced again after it resets.
bool g_logo_skipped = false;

bool IsLogoManager(uint8_t* base, uint32_t mgr) {
    return mgr != 0 && X86_MEM32(mgr + 0x14) == kLogoTable;
}

// Calls a player object's destructor (vtable + 0x04) exactly like the playlist completion does.
void ReleaseObject(X86Context& ctx, uint8_t* base, uint32_t obj) {
    if (!obj) return;
    uint32_t vt = X86_MEM32(obj);
    if (!vt) return;
    uint32_t fn = X86_MEM32(vt + 4);
    if (!fn) return;
    X86_PUSH32(0);  // the guest `ret` pops this and returns to the caller of x86_icall
    ctx.ecx = obj;
    x86_icall(ctx, base, fn);
}

}  // namespace

// The playlist setup starts a new boot; allow the next playlist to be advanced again.
X86_FUNC(sub_00028940) {
    g_logo_skipped = false;
    __imp__sub_00028940(ctx, base);
}

X86_FUNC(sub_00028B10) {
    uint32_t mgr = ctx.ecx;
    if (app::Get().skip_intro && !g_logo_skipped && IsLogoManager(base, mgr)) {
        g_logo_skipped = true;
        uint32_t phase = X86_MEM32(mgr + 0x78);
        LOGI("skip_intro: playlist forced to completion from phase %u index %u", phase, X86_MEM32(mgr + 0x1C));
        if (phase >= 1 && phase <= 4) {
            // A movie is already playing: release the same objects the normal completion releases.
            ReleaseObject(ctx, base, X86_MEM32(mgr + 0x74));
            ReleaseObject(ctx, base, X86_MEM32(mgr + 0x10));
            ReleaseObject(ctx, base, X86_MEM32(mgr + 0x70));
            X86_MEM32(mgr + 0x74) = 0;
            X86_MEM32(mgr + 0x10) = 0;
            X86_MEM32(mgr + 0x70) = 0;
            X86_MEM8(mgr + 0x68) = 0;
        }
        ctx.ecx = mgr;
        X86_MEM32(mgr + 0x78) = 5;
    }
    __imp__sub_00028B10(ctx, base);
}
