#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikapp.h>
#include <eikon.hrh>
#include <e32svr.h>
#include <f32file.h>
#include "probe.h"
const TUid KProbeUid={0x0F700661};
static void LogLine(const TDesC& line){
    RFs fs;if(fs.Connect()!=KErrNone)return;
    RFile file;TInt error=file.Open(fs,_L("C:\\KeyHookProbe.log"),EFileWrite|EFileShareAny);
    if(error==KErrNotFound)error=file.Create(fs,_L("C:\\KeyHookProbe.log"),EFileWrite|EFileShareAny);
    if(error==KErrNone){TInt pos=0;file.Seek(ESeekEnd,pos);TBuf8<160> bytes;bytes.Copy(line.Left(150));bytes.Append(_L8("\r\n"));file.Write(bytes);file.Flush();file.Close();}
    fs.Close();
}
struct KernelBackend {
    void Mark(const char* message){TBuf<120> text;while(*message)text.Append((TChar)*message++);LogLine(text);}
    void Capture(){UserSvr::CaptureEventHook();}
    void Release(){UserSvr::ReleaseEventHook();}
};
static TInt Worker(TAny* parameter){KernelBackend backend;RunProbe(backend,*static_cast<ProbeState*>(parameter));return KErrNone;}
class CProbeView:public CCoeControl {
public:
    CProbeView():iTimer(0),iRunning(EFalse),iAttempted(EFalse),iTimeout(EFalse),iTicks(0),iCloseTicks(0),iExitRequested(EFalse),iOldJit(EFalse){}
    void ConstructL(){
        CreateWindowL();SetExtentToWholeScreen();ActivateL();
        iOldJit=User::JustInTime();User::SetJustInTime(EFalse);
        iText=_L("KEY HOOK PROBE 0.1.0\n5 / left key: RUN\nRight key: EXIT\n\nCapture then release.\nNo raw events read.\nKernel may reject or\npanic worker thread.\nKeyboard recovery is\nnot guaranteed.\nLog C:\\KeyHookProbe.log");
        iTimer=CPeriodic::NewL(CActive::EPriorityStandard);iTimer->Start(100000,100000,TCallBack(Tick,this));
    }
    ~CProbeView(){delete iTimer;if(iRunning)iThread.Kill(KErrCancel);iThread.Close();User::SetJustInTime(iOldJit);}
    void Start(){
        if(iAttempted)return;iAttempted=ETrue;
        LogLine(_L("--- KeyHookProbe 0.1.0 BEGIN ---"));
        iState.phase=ProbeIdle;
        TInt error=iThread.Create(_L("KeyHookProbeWorker"),Worker,8192,4096,65536,&iState);
        if(error!=KErrNone){iText.Format(_L("THREAD CREATE FAILED\nReason %d\nNo hook called."),error);LogLine(iText);DrawNow();return;}
        iRunning=ETrue;iTicks=0;
        iText=_L("TEST RUNNING\nWorker before capture.\nTimeout about 5s.\nNo keys needed.\n\nLog flushed before\nCaptureEventHook.");DrawNow();
        iThread.Resume();
    }
    TBool Running()const{return iRunning;}
    void RequestExit(){if(iRunning){iExitRequested=ETrue;return;}iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);}
    TKeyResponse OfferKeyEventL(const TKeyEvent& key,TEventCode type){
        if(type!=EEventKey||key.iRepeats>0)return EKeyWasConsumed;
        if(key.iScanCode==EStdKeyDevice1||key.iCode=='#'||key.iCode==EKeyEscape){RequestExit();return EKeyWasConsumed;}
        if(key.iCode=='5'||key.iCode==EKeyEnter||key.iScanCode==EStdKeyDevice0||key.iScanCode==EStdKeyDevice3){Start();return EKeyWasConsumed;}
        return EKeyWasNotConsumed;
    }
private:
    static TInt Tick(TAny* ptr){return static_cast<CProbeView*>(ptr)->Poll();}
    TInt Poll(){
        if(iRunning){
            ++iTicks;
            if(iThread.ExitType()!=EExitPending){
                TExitType type=iThread.ExitType();TInt reason=iThread.ExitReason();TExitCategoryName category=iThread.ExitCategory();
                iText.Format(_L("KEY HOOK RESULT\nPhase %d\nExit type %d\nReason %d\nCategory:\n%S\n\n1: before capture\n2: capture returned\n3: release returned\nAuto close about 30s"),iState.phase,(TInt)type,reason,&category);
                LogLine(iText);iRunning=EFalse;iThread.Close();iCloseTicks=0;DrawNow();
            }else if(iTicks>=50&&!iTimeout){
                iTimeout=ETrue;iText.Format(_L("TIMEOUT\nPhase %d\nWorker kill requested.\nKernel hang cannot\nbe recovered by timer."),iState.phase);
                LogLine(iText);DrawNow();iThread.Kill(KErrTimedOut);
            }
        }else if(iAttempted){
            if(iExitRequested||++iCloseTicks>=300){iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return 0;}
        }
        return 1;
    }
    void Draw(const TRect&)const{
        CWindowGc& gc=SystemGc();gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));gc.Clear(Rect());
        gc.SetPenColor(TRgb(220,239,250));gc.UseFont(iEikonEnv->LegendFont());
        TInt start=0,y=15;
        for(TInt i=0;i<=iText.Length()&&y<202;++i)if(i==iText.Length()||iText[i]=='\n'){
            gc.DrawText(iText.Mid(start,i-start),TPoint(3,y));y+=16;start=i+1;
        }
        gc.DiscardFont();
    }
    CPeriodic* iTimer;RThread iThread;ProbeState iState;TBool iRunning,iAttempted,iTimeout;
    TInt iTicks,iCloseTicks;TBool iExitRequested,iOldJit;TBuf<600> iText;
};
class CProbeUi:public CAknAppUi {
public:CProbeUi():iView(0){}void ConstructL(){BaseConstructL();iView=new(ELeave)CProbeView;iView->ConstructL();AddToStackL(iView);iView->SetFocus(ETrue);}
    ~CProbeUi(){if(iView){RemoveFromStack(iView);delete iView;}}
    void HandleCommandL(TInt command){if(command==0x7101){iView->Start();return;}if(command==EAknSoftkeyExit){iView->RequestExit();return;}if(command==EEikCmdExit)Exit();}
private:CProbeView* iView;
};
class CProbeDocument:public CAknDocument {public:CProbeDocument(CEikApplication& app):CAknDocument(app){}private:CEikAppUi* CreateAppUiL(){return new(ELeave)CProbeUi;}};
class CProbeApplication:public CAknApplication {private:TUid AppDllUid()const{return KProbeUid;}CApaDocument* CreateDocumentL(){return new(ELeave)CProbeDocument(*this);}};
EXPORT_C CApaApplication* NewApplication(){return new CProbeApplication;}
GLDEF_C TInt E32Dll(TDllReason){return KErrNone;}
