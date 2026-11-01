#ifndef KEYHOOK_PROBE_H
#define KEYHOOK_PROBE_H
struct ProbeState {volatile int phase;ProbeState():phase(0){}};
enum { ProbeIdle,ProbeBeforeCapture,ProbeCaptureReturned,ProbeReleaseReturned };
template<class Backend> void RunProbe(Backend& backend,ProbeState& state){
    state.phase=ProbeBeforeCapture;backend.Mark("BEFORE CaptureEventHook");
    backend.Capture();
    state.phase=ProbeCaptureReturned;
    // Deliberately do not RequestEvent: first test ownership acquisition only.
    backend.Release();
    state.phase=ProbeReleaseReturned;
    backend.Mark("CaptureEventHook RETURNED (void)");
    backend.Mark("ReleaseEventHook RETURNED (void)");
}
#endif
