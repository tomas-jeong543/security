// Benign, uniquely named module used only to verify the unchanged DLL scanner.
// No entry point or CRT dependencies: loading it adds only this module.
extern "C" __declspec(dllexport) int HK01Probe() { return 1; }
