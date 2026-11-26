// PR66 ABI candidate only. No engine access, configuration, I/O, threads or hooks.
// No custom DllMain, no global constructors, no active capabilities.
extern "C" int __cdecl SSCInitialize() noexcept {
    return 1; // Successfully initialized an inert component, not a loader gate pass.
}
