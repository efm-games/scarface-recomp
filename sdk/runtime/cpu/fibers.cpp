// Cooperative guest fibers.
//
// The game runs several guest threads' worth of work as cooperative fibers on one host thread. The switch function
// (sub_003094E8 in Scarface) saves esp into the current descriptor, loads esp from the target descriptor, pops the
// target's callee saved registers and returns. On the real console that return lands in a different guest stack.
//
// Recompiled code mirrors the guest call chain with host frames, so switching only ctx.esp would leave the host
// return address pointing back into the switch function's C++ caller. Instead each guest descriptor gets a host
// fiber. Switching guest stacks switches host stacks too, which keeps every fiber's recompiled call chain intact.
//
// The recompiler detects the switch function and replaces its ret with x86_pivot_ret. At that ret ctx.esp is already
// the target stack, ctx.eax is the target descriptor and ctx.ecx the source descriptor.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <unordered_map>

#include "core/log.h"
#include "cpu/x86_ops.h"

namespace dispatch {
X86Func* Find(uint32_t guest);
}

namespace {

#ifdef _WIN32

// Host frames are larger than the guest's, and a fiber can be suspended as deep as its guest stack allows.
constexpr SIZE_T kFiberStack = 8u << 20;

struct FiberState {
    uint32_t desc = 0;       // guest descriptor address
    uint32_t entry = 0;      // first guest address of a new fiber; always the trampoline
    X86Context* ctx = nullptr;
    uint8_t* base = nullptr;
    bool started = false;    // the host fiber has run, so a switch to it resumes a host stack
    void* handle = nullptr;  // CreateFiber/ConvertThreadToFiber handle
};

thread_local std::unordered_map<uint32_t, FiberState*> g_fibers;
thread_local FiberState* g_current = nullptr;

void CALLBACK FiberEntry(void* param) {
    FiberState* s = static_cast<FiberState*>(param);
    g_current = s;
    LOGI("fiber %08X starts at %08X (esp %08X)", s->desc, s->entry, s->ctx->esp);
    if (X86Func* f = dispatch::Find(s->entry)) f(*s->ctx, s->base);
    x86_trap(*s->ctx, s->entry, "guest fiber function returned");
}

// the first switch on a host thread: make the thread itself a fiber so guest fibers can switch back to it
void EnsureCurrent(X86Context& ctx, uint8_t* base) {
    if (g_current) return;
    void* handle = ConvertThreadToFiber(nullptr);
    if (!handle) {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_FIBER) handle = GetCurrentFiber();
        if (!handle) {
            LOGE("fiber: ConvertThreadToFiber failed (%lu)", (unsigned long)err);
            x86_trap(ctx, 0, "ConvertThreadToFiber failed");
        }
    }
    FiberState* s = new FiberState;
    s->desc = ctx.ecx;  // the source descriptor at the pivot is the current guest fiber
    s->ctx = &ctx;
    s->base = base;
    s->started = true;
    s->handle = handle;
    if (s->desc) g_fibers[s->desc] = s;
    g_current = s;
    LOGI("fiber %08X is the host thread", s->desc);
}

FiberState* EnsureFiber(uint32_t desc, X86Context& ctx, uint8_t* base) {
    auto it = g_fibers.find(desc);
    if (it != g_fibers.end()) return it->second;
    if (!desc) {
        LOGE("fiber: switch to descriptor 0 from %08X", g_current ? g_current->desc : 0);
        x86_trap(ctx, 0, "switch to a null fiber descriptor");
    }
    FiberState* s = new FiberState;
    s->desc = desc;
    s->ctx = &ctx;
    s->base = base;
    s->handle = CreateFiber(kFiberStack, FiberEntry, s);
    if (!s->handle) {
        LOGE("fiber: CreateFiber failed (%lu)", (unsigned long)GetLastError());
        x86_trap(ctx, 0, "CreateFiber failed");
    }
    g_fibers[desc] = s;
    LOGI("fiber %08X created", desc);
    return s;
}

#endif  // _WIN32

}  // namespace

void x86_pivot_ret(X86Context& ctx, uint8_t* base, int n) {
#ifdef _WIN32
    EnsureCurrent(ctx, base);
    // the switch function has already loaded the target stack; finish its `ret n`
    const uint32_t target = X86_MEM32(ctx.esp);
    ctx.esp += uint32_t(4 + n);
    FiberState* self = g_current;
    FiberState* dst = EnsureFiber(ctx.eax, ctx, base);
    if (dst == self) return;
    dst->ctx = &ctx;
    dst->base = base;
    if (!dst->started) {
        // a brand new fiber starts at the return address its initial frame was built with (the trampoline)
        dst->entry = target;
        dst->started = true;
    }
    g_current = dst;
    SwitchToFiber(dst->handle);
    // another fiber switched back: g_current is self again and ctx already holds this fiber's restored state
#else
    // Windows first; other hosts keep the plain guest ret for now
    (void)base;
    ctx.esp += uint32_t(4 + n);
#endif
}
