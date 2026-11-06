#include "../src/probe.h"
#include <assert.h>
#include <stdio.h>
struct Backend{int capture,release,marks,fail;Backend(int f):capture(0),release(0),marks(0),fail(f){}void Mark(const char*){++marks;}void Capture(){++capture;if(fail==1)throw 1;}void Release(){++release;if(fail==2)throw 2;}};
int main(){for(int fail=0;fail<3;++fail){ProbeState state;Backend backend(fail);try{RunProbe(backend,state);}catch(int e){assert(e==fail);}assert(backend.capture==1);assert(backend.release==(fail==1?0:1));assert(state.phase==(fail==1?ProbeBeforeCapture:fail==2?ProbeCaptureReturned:ProbeReleaseReturned));}puts("PASS: capture/release ordering and failure checkpoints; no RequestEvent is used.");}
