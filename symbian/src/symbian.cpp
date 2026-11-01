#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikapp.h>
#include <eikon.hrh>
#include <fbs.h>
#include <bitstd.h>
#include <bitdev.h>
#include <w32std.h>
#include <e32keys.h>
#include "soft3d.h"
#include "menu.h"
#include "runtime.h"

const TUid KSkylinerUid={0x0F700660};
class CFlightView:public CCoeControl,public MDirectScreenAccess {
public:
    CFlightView():iBitmap(0),iMenuBitmap(0),iTimer(0),iRenderer(0),iPixels(0),iDsa(0),iDsaReady(EFalse),iDsaSuspended(EFalse),iKeys(0),iPaused(ETrue),iAccum(0),iFps(0),iRenderMs(0),iPresentMs(0) {}
    void ConstructL() {
        CreateWindowL();SetExtentToWholeScreen();
        iBitmap=new(ELeave)CFbsBitmap;
        User::LeaveIfError(iBitmap->Create(TSize(SKY_DISPLAY_W,SKY_DISPLAY_H),EColor64K));
        iMenuBitmap=new(ELeave)CFbsBitmap;
        User::LeaveIfError(iMenuBitmap->Create(TSize(SKY_DISPLAY_W,SKY_DISPLAY_H),EColor64K));
        iPixels=new(ELeave)Pixel[SKY_W*SKY_H];
        Mem::FillZ(iPixels,SKY_W*SKY_H*sizeof(Pixel));
        iRenderer=new(ELeave)Soft3D;
        FlightModel_Reset(&iFlight);
        iLast.UniversalTime();iFpsCounter.Reset();
        BuildMenu();
        ActivateL();
        // DSA is the only presentation backend in this build.
        StartDsaL();Present();
        iTimer=CPeriodic::NewL(CActive::EPriorityIdle);
        // Do not hide faster presentation behind the old 50ms timer delay.
        // Physics retains its independent 20ms fixed step.
        iTimer->Start(10000,10000,TCallBack(Tick,this));
    }
    ~CFlightView(){iRepeatGuard.Restore(iEikonEnv->WsSession());delete iTimer;delete iDsa;delete iRenderer;delete iBitmap;delete iMenuBitmap;delete[] iPixels;}
    void UpdateRepeatPolicy(){
        iRepeatGuard.Update(!iPaused&&iDsaReady&&!iDsaSuspended&&IsFocused(),iEikonEnv->WsSession());
    }
    void StartDsaL(){
        if(!iDsa)iDsa=CDirectScreenAccess::NewL(iEikonEnv->WsSession(),*iEikonEnv->ScreenDevice(),Window(),*this);
        iDsa->StartL(); // Refreshes both origin and clipping region.
        iDsaReady=ETrue;
    }
    void AbortNow(RDirectScreenAccess::TTerminationReasons){
        // No Window Server calls here: the server is waiting for our return.
        // Never cache Gc()/ScreenDevice() across an abort/restart.
        iDsaReady=EFalse;iDsaSuspended=ETrue;
        iKeys=0;iAccum=0;iPaused=ETrue;iMenu.Pause();
    }
    void Restart(RDirectScreenAccess::TTerminationReasons){
        TRAPD(error,StartDsaL());
        iDsaReady=(error==KErrNone);iDsaSuspended=EFalse;
        if(error!=KErrNone){iEikonEnv->HandleError(error);return;}
        iLast.UniversalTime();iFpsCounter.Reset();UpdateRepeatPolicy();BuildMenu();Present();
    }
    void Present(){
        if(iDsaSuspended)return;
        if(iDsaReady){
            Paint(*iDsa->Gc());
            iDsa->ScreenDevice()->Update(*iDsa->DrawingRegion());
        }
    }
    void BuildMenu(){
        if(!iMenuBitmap||!iMenu.Visible())return;
        iMenuBitmap->LockHeap(ETrue);
        SkyDrawMenu((Pixel*)iMenuBitmap->DataAddress(),CFbsBitmap::ScanLineLength(SKY_DISPLAY_W,EColor64K)/2,iMenu,iPixels,SKY_W);
        iMenuBitmap->UnlockHeap(ETrue);
    }
    void ApplyMenu(MenuState::Action action){
        if(action==MenuState::Exit){iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return;}
        if(action==MenuState::Start){ResetFlight();return;}
        iPaused=iMenu.Visible();iKeys=0;iAccum=0;iLast.UniversalTime();iFpsCounter.Reset();UpdateRepeatPolicy();BuildMenu();Present();
    }
    void ToggleMenu(){
        if(iDsaSuspended)return;
        if(!iDsaReady){TRAPD(error,StartDsaL());if(error!=KErrNone){iEikonEnv->HandleError(error);return;}}
        if(iMenu.Visible()){ApplyMenu(iMenu.Select());return;}
        iMenu.Pause();iPaused=ETrue;iKeys=0;iAccum=0;iFpsCounter.Reset();UpdateRepeatPolicy();BuildMenu();Present();
    }
    void ResetFlight(){FlightModel_Reset(&iFlight);iMenu.mode=MenuState::Flight;iMenu.selection=0;iKeys=0;iAccum=0;iPaused=EFalse;iLast.UniversalTime();iFpsCounter.Reset();UpdateRepeatPolicy();}
    static TInt Tick(TAny* p){return static_cast<CFlightView*>(p)->Frame();}
    TInt Frame(){
        // DSA AbortNow defers repeat restoration here: no WS calls in abort.
        UpdateRepeatPolicy();
        if(iDsaSuspended||!iDsaReady)return 1;
        TTime now;now.UniversalTime();
        TInt elapsed=now.MicroSecondsFrom(iLast).Int64().GetTInt();iLast=now;
        // Keep the last frame behind the menu; do not spend a second rendering
        // while the player is trying to resume or exit.
        if(iPaused)return 1;
        iFps=iFpsCounter.Add(elapsed);
        if(elapsed<0)elapsed=0;if(elapsed>250000)elapsed=250000;
        if(!iPaused){
            iAccum+=elapsed;
            FlightInput in={0};
            in.pitch=((iKeys&2)?Fix::Raw(65536):0)-((iKeys&1)?Fix::Raw(65536):0);
            in.roll=((iKeys&8)?Fix::Raw(65536):0)-((iKeys&4)?Fix::Raw(65536):0);
            in.throttle=((iKeys&16)?Fix::Raw(65536):0)-((iKeys&32)?Fix::Raw(65536):0);
            in.yaw=((iKeys&64)?Fix::Raw(65536):0)-((iKeys&128)?Fix::Raw(65536):0);
            while(iAccum>=20000){FlightModel_Step(&iFlight,&in,Fix::Raw(1311));iAccum-=20000;}
        }
        TTime renderStart;renderStart.UniversalTime();
        iRenderer->Render(iPixels,SKY_W,iFlight.state);
        TTime renderEnd;renderEnd.UniversalTime();
        iRenderMs=renderEnd.MicroSecondsFrom(renderStart).Int64().GetTInt()/1000;
        TTime presentStart;presentStart.UniversalTime();
        iBitmap->LockHeap(ETrue);
        SkyScale2x(iPixels,SKY_W,(Pixel*)iBitmap->DataAddress(),CFbsBitmap::ScanLineLength(SKY_DISPLAY_W,EColor64K)/2);
        iBitmap->UnlockHeap(ETrue);
        Present();
        TTime presentEnd;presentEnd.UniversalTime();
        iPresentMs=presentEnd.MicroSecondsFrom(presentStart).Int64().GetTInt()/1000;
        return 1;
    }
    TKeyResponse OfferKeyEventL(const TKeyEvent& key,TEventCode type){
        // No physics or rendering for typematic repeats; queued repeats may
        // still arrive just after a rate change or foreground transition.
        if(type==EEventKey&&key.iRepeats>0)return EKeyWasConsumed;
        if((type==EEventKeyDown||type==EEventKey)&&
           (key.iScanCode==EStdKeyDevice1||key.iScanCode==EStdKeyHash||key.iCode==EKeyEscape||key.iCode=='#')){
            iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return EKeyWasConsumed;
        }
        if(type==EEventKey&&iMenu.Visible()){
            if(key.iCode==EKeyUpArrow||key.iCode=='2'){iMenu.Move(-1);BuildMenu();Present();return EKeyWasConsumed;}
            if(key.iCode==EKeyDownArrow||key.iCode=='8'){iMenu.Move(1);BuildMenu();Present();return EKeyWasConsumed;}
            if(key.iCode=='5'||key.iCode==EKeyEnter||key.iScanCode==EStdKeyDevice3||key.iScanCode==EStdKeyDevice0){ApplyMenu(iMenu.Select());return EKeyWasConsumed;}
        }
        if(type==EEventKey&&(key.iCode=='5'||key.iScanCode==EStdKeyDevice0||key.iScanCode==EStdKeyDevice3)){
            ToggleMenu();return EKeyWasConsumed;
        }
        if(type==EEventKey&&key.iCode=='0'){ResetFlight();return EKeyWasConsumed;}
        if(iPaused)return EKeyWasConsumed;
        TInt mask=0;
        switch(key.iScanCode){
        case EStdKeyUpArrow:mask=1;break;case EStdKeyDownArrow:mask=2;break;
        case EStdKeyLeftArrow:mask=4;break;case EStdKeyRightArrow:mask=8;break;
        case '3':mask=16;break;case '1':mask=32;break;
        case '9':mask=64;break;case '7':mask=128;break;
        }
        if(mask){if(type==EEventKeyDown)iKeys|=mask;else if(type==EEventKeyUp)iKeys&=~mask;return EKeyWasConsumed;}
        return EKeyWasNotConsumed;
    }
    void FocusChanged(TDrawNow){if(!IsFocused()){iKeys=0;iPaused=ETrue;iAccum=0;iFpsCounter.Reset();iMenu.Pause();}if(!iDsaSuspended)UpdateRepeatPolicy();}
    void Draw(const TRect&) const {
        // Window Server redraw events do not present game frames.
        // DSA Restart/Present owns all scene and menu drawing.
    }
    void Paint(CBitmapContext& gc) const {
        if(iMenu.Visible()){gc.BitBlt(TPoint(0,0),iMenuBitmap);return;}
        gc.BitBlt(TPoint(0,0),iBitmap);
        gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));
        gc.SetPenStyle(CGraphicsContext::ENullPen);gc.DrawRect(TRect(2,2,174,31));
        gc.SetPenStyle(CGraphicsContext::ESolidPen);gc.SetPenColor(TRgb(220,239,250));gc.UseFont(iEikonEnv->LegendFont());
        TBuf<64> text;
        text.Format(_L("SPD %d   ALT %d"),FxInt(iFlight.state.speed*Fix::Raw(235930)),FxInt(iFlight.state.altitude));
        gc.DrawText(text,TPoint(6,14));
        text.Format(_L("THR %d%%   FPS %d"),FxInt(iFlight.state.throttle*100),iFps);
        gc.DrawText(text,TPoint(6,27));
        gc.DrawText(_L("MENU"),TPoint(4,SKY_DISPLAY_H-5));
        gc.DrawText(_L("EXIT"),TPoint(SKY_DISPLAY_W-30,SKY_DISPLAY_H-5));
        gc.DiscardFont();
    }

private:
    CFbsBitmap* iBitmap;CFbsBitmap* iMenuBitmap;MenuState iMenu;CPeriodic* iTimer;Soft3D* iRenderer;Pixel* iPixels;
    CDirectScreenAccess* iDsa;TBool iDsaReady;TBool iDsaSuspended;
    KeyRepeatGuard<RWsSession,TTimeIntervalMicroSeconds32> iRepeatGuard;FrameRateCounter iFpsCounter;
    FlightModel iFlight;TInt iKeys;TBool iPaused;TInt iAccum;TInt iFps;TInt iRenderMs;TInt iPresentMs;TTime iLast;
};
class CFlightUi:public CAknAppUi {
public:
    CFlightUi():iView(0){}
    void ConstructL(){BaseConstructL();SetKeyBlockMode(ENoKeyBlock);iView=new(ELeave)CFlightView;iView->ConstructL();AddToStackL(iView);iView->SetFocus(ETrue);}
    ~CFlightUi(){if(iView){RemoveFromStack(iView);delete iView;}}
    void HandleCommandL(TInt cmd){if(cmd==EEikCmdExit||cmd==EAknSoftkeyExit){Exit();return;}if(cmd==0x7001&&iView)iView->ToggleMenu();}
private:CFlightView* iView;
};
class CFlightDocument:public CAknDocument {
public:CFlightDocument(CEikApplication& a):CAknDocument(a){}
private:CEikAppUi* CreateAppUiL(){return new(ELeave)CFlightUi;}
};
class CFlightApplication:public CAknApplication {
private:
    TUid AppDllUid()const{return KSkylinerUid;}
    CApaDocument* CreateDocumentL(){return new(ELeave)CFlightDocument(*this);}
};
EXPORT_C CApaApplication* NewApplication(){return new CFlightApplication;}
GLDEF_C TInt E32Dll(TDllReason){return KErrNone;}
