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
#include <hal.h>
#include <e32svr.h>
#include "soft3d.h"
#include "menu.h"
#include "runtime.h"
#include "framebuffer.h"

const TUid KSkylinerUid={0x0F700660};
class CFlightView:public CCoeControl,public MDirectScreenAccess {
public:
    CFlightView():iMenuBitmap(0),iRenderer(0),iPixels(0),iDsa(0),iDsaReady(EFalse),iDsaSuspended(EFalse),iScreenBase(0),iScreenStride(0),iMapError(0),iKeys(0),iPaused(ETrue),iAccum(0),iFps(0),iRenderMs(0),iPresentMs(0) {}
    void ConstructL() {
        CreateWindowL();SetExtentToWholeScreen();
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

    }
    ~CFlightView(){iRepeatGuard.Restore(iEikonEnv->WsSession());delete iDsa;delete iRenderer;delete iMenuBitmap;delete[] iPixels;}
    void UpdateRepeatPolicy(){
        iRepeatGuard.Update(!iPaused&&iDsaReady&&!iDsaSuspended&&IsFocused(),iEikonEnv->WsSession());
    }
    void StartDsaL(){
        if(!iDsa)iDsa=CDirectScreenAccess::NewL(iEikonEnv->WsSession(),*iEikonEnv->ScreenDevice(),Window(),*this);
        iScreenBase=0;iScreenStride=0;iDsaReady=EFalse;
        iDsa->StartL(); // Refreshes both origin and clipping region.
        TRAPD(mapError,MapScreenL());iMapError=mapError;
        iDsaReady=ETrue; // Diagnostic GC remains usable when raw mapping is unsupported.
    }
    void AbortNow(RDirectScreenAccess::TTerminationReasons){
        // No Window Server calls here: the server is waiting for our return.
        // Never cache Gc()/ScreenDevice() across an abort/restart.
        iScreenBase=0;iScreenStride=0;iDsaReady=EFalse;iDsaSuspended=ETrue;
        iButtons.Clear();iKeys=0;iAccum=0;iPaused=ETrue;iMenu.Pause();
    }
    void Restart(RDirectScreenAccess::TTerminationReasons){
        TRAPD(error,StartDsaL());
        iDsaReady=(error==KErrNone);iDsaSuspended=EFalse;
        if(error!=KErrNone){iEikonEnv->HandleError(error);return;}
        iLast.UniversalTime();iFpsCounter.Reset();UpdateRepeatPolicy();BuildMenu();Present();
    }
    void DisplayLog(const TDesC8& text){
        RFile log;if(log.Replace(iEikonEnv->FsSession(),_L("C:\\Skyliner700-display.log"),EFileWrite)==KErrNone){log.Write(text);log.Close();}
    }
    void MapScreenL(){
        Mem::FillZ(&iLegacyScreen,sizeof(iLegacyScreen));
        TPckg<TScreenInfoV01> package(iLegacyScreen);UserSvr::ScreenInfo(package);
        const HALData::TAttribute attributes[9]={HALData::EDisplayMode,HALData::EDisplayMemoryAddress,
            HALData::EDisplayOffsetToFirstPixel,HALData::EDisplayOffsetBetweenLines,
            HALData::EDisplayBitsPerPixel,HALData::EDisplayIsPixelOrderRGB,
            HALData::EDisplayIsPixelOrderLandscape,HALData::EDisplayXPixels,HALData::EDisplayYPixels};
        TInt firstError=KErrNone;TBuf8<768> report;
        for(TInt n=0;n<9;++n){
            iDisplayValues[n]=(n>=2&&n<=4&&iDisplayErrors[0]==KErrNone)?iDisplayValues[0]:0;
            iDisplayErrors[n]=HAL::Get(attributes[n],iDisplayValues[n]);
            TBuf8<80> line;line.Format(_L8("attribute %d value %08x error %d\r\n"),
                (TInt)attributes[n],(TUint)iDisplayValues[n],iDisplayErrors[n]);report.Append(line);
            if(firstError==KErrNone&&iDisplayErrors[n]!=KErrNone)firstError=iDisplayErrors[n];
        }
        TBuf8<96> legacy;
        legacy.Format(_L8("ScreenInfo valid %d base %08x size %dx%d\r\n"),iLegacyScreen.iScreenAddressValid,
            (TUint)iLegacyScreen.iScreenAddress,iLegacyScreen.iScreenSize.iWidth,iLegacyScreen.iScreenSize.iHeight);
        report.Append(legacy);DisplayLog(report);User::LeaveIfError(firstError);
        const TInt mode=iDisplayValues[0],address=iDisplayValues[1],offset=iDisplayValues[2];
        const TInt pitch=iDisplayValues[3],bpp=iDisplayValues[4],rgb=iDisplayValues[5];
        const TInt landscape=iDisplayValues[6],width=iDisplayValues[7],height=iDisplayValues[8];
        // LAND is a HAL pixel-order flag, not the FBS graphics orientation.
        // Nokia 6600 reports LAND=1 with 176x208, RGB565, pitch=352, FBS ORI=0.
        // Use the actual dimensions/pitch and FBS orientation for address mapping.
        // Keep the raw flag in diagnostics, but do not reject this measured layout.
        if(!address||offset<0||offset>65536||pitch<width*2||pitch>8192||
           (pitch&1)||bpp!=16||!rgb||width!=SKY_DISPLAY_W||height!=SKY_DISPLAY_H||
           iDsa->ScreenDevice()->DisplayMode()!=EColor64K||
           iDsa->ScreenDevice()->Orientation()!=CFbsBitGc::EGraphicsOrientationNormal)
            User::Leave(KErrNotSupported);
        const TUint base=(TUint)address,first=base+(TUint)offset;
        if(first<base||(first&1)||first+(TUint)(pitch*height)<first)User::Leave(KErrArgument);
        iScreenOrigin=PositionRelativeToScreen();
        if(iScreenOrigin.iX<0||iScreenOrigin.iY<0||
           iScreenOrigin.iX+SKY_DISPLAY_W>width||iScreenOrigin.iY+SKY_DISPLAY_H>height)
            User::Leave(KErrNotSupported);
        iScreenStride=pitch>>1;iScreenBase=(Pixel*)first;
    }
    void Present(){
        if(iDsaSuspended||!iDsaReady)return;
        if(iMapError){PaintDisplayDiagnostic(*iDsa->Gc());iDsa->ScreenDevice()->Update(*iDsa->DrawingRegion());return;}
        if(!iScreenBase)return;
        const RRegion* region=iDsa->DrawingRegion();
        if(!region||region->CheckError()||region->IsEmpty())return;
        const TBool menu=iMenu.Visible();
        const Pixel* source=iPixels;TInt sourceStride=SKY_W;
        if(menu){
            iMenuBitmap->LockHeap(ETrue);source=(const Pixel*)iMenuBitmap->DataAddress();
            sourceStride=CFbsBitmap::ScanLineLength(SKY_DISPLAY_W,EColor64K)>>1;
        }
        Pixel* screen=iScreenBase+iScreenOrigin.iY*iScreenStride+iScreenOrigin.iX;
        const TRect* rectangles=region->RectangleList();
        for(TInt n=0;n<region->Count();++n){
            TRect r=rectangles[n];r.Move(-iScreenOrigin.iX,-iScreenOrigin.iY);
            SkyPresentRect(source,sourceStride,screen,iScreenStride,
                           r.iTl.iX,r.iTl.iY,r.iBr.iX,r.iBr.iY,!menu);
        }
        if(menu)iMenuBitmap->UnlockHeap(ETrue);
        else PaintHud(*iDsa->Gc());
        iDsa->ScreenDevice()->Update(*iDsa->DrawingRegion());
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
    TBool ContinuousFrames()const{return !iMapError&&!iPaused&&iDsaReady&&!iDsaSuspended&&IsFocused();}
    TInt Frame(){
        // DSA AbortNow defers repeat restoration here: no WS calls in abort.
        UpdateRepeatPolicy();
        if(iDsaSuspended||!iDsaReady)return 1;
        const FlightButtons::Snapshot buttons=iButtons.Take();
        iKeys=buttons.held&255;
        // Actions run once per frame, never inside the WS event callback.
        // Exit can destroy this view: return immediately after requesting it.
        if(buttons.pressed&FlightButtons::Exit){iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return 1;}
        if(iMapError)return 1;
        if(buttons.pressed&FlightButtons::Select){ToggleMenu();return 1;}
        if(iMenu.Visible()){
            if(buttons.pressed&(FlightButtons::PitchUp|FlightButtons::MenuUp))iMenu.Move(-1);
            if(buttons.pressed&(FlightButtons::PitchDown|FlightButtons::MenuDown))iMenu.Move(1);
            if(buttons.pressed&(FlightButtons::PitchUp|FlightButtons::MenuUp|FlightButtons::PitchDown|FlightButtons::MenuDown)){BuildMenu();Present();}
        }else if(buttons.pressed&FlightButtons::Reset){ResetFlight();return 1;}
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
        Present();
        TTime presentEnd;presentEnd.UniversalTime();
        iPresentMs=presentEnd.MicroSecondsFrom(presentStart).Int64().GetTInt()/1000;
        return 1;
    }
    TBool HandleGameKey(const TKeyEvent& key,TInt type){
        unsigned mask=0;
        switch(key.iScanCode){
        case EStdKeyUpArrow:mask=FlightButtons::PitchUp;break;
        case EStdKeyDownArrow:mask=FlightButtons::PitchDown;break;
        case EStdKeyLeftArrow:mask=FlightButtons::RollLeft;break;
        case EStdKeyRightArrow:mask=FlightButtons::RollRight;break;
        case '3':mask=FlightButtons::ThrottleUp;break;case '1':mask=FlightButtons::ThrottleDown;break;
        case '9':mask=FlightButtons::YawRight;break;case '7':mask=FlightButtons::YawLeft;break;
        case '2':mask=FlightButtons::MenuUp;break;case '8':mask=FlightButtons::MenuDown;break;
        case '5':case EStdKeyEnter:case EStdKeyDevice0:case EStdKeyDevice3:mask=FlightButtons::Select;break;
        case EStdKeyDevice1:case EStdKeyHash:case EStdKeyEscape:mask=FlightButtons::Exit;break;
        case '0':mask=FlightButtons::Reset;break;
        }
        if(!mask)return EFalse;
        // Ignore translated EEventKey, including autorepeat, before Avkon.
        // Preserve physical held bits in menus so holding Select cannot toggle twice.
        if(!iDsaSuspended&&iDsaReady&&IsFocused()){
            if(type==EEventKeyDown)iButtons.ChangeScan(key.iScanCode,mask,true);
            else if(type==EEventKeyUp)iButtons.ChangeScan(key.iScanCode,mask,false);
        }
        return ETrue;
    }
    void FocusChanged(TDrawNow){if(!IsFocused()){iButtons.Clear();iKeys=0;iPaused=ETrue;iAccum=0;iFpsCounter.Reset();iMenu.Pause();}if(!iDsaSuspended)UpdateRepeatPolicy();}
    void Draw(const TRect&) const {
        // Window Server redraw events do not present game frames.
        // DSA Restart/Present owns all scene and menu drawing.
    }
    void PaintDisplayDiagnostic(CBitmapContext& gc)const{
        gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));
        gc.SetPenStyle(CGraphicsContext::ENullPen);gc.DrawRect(TRect(0,0,176,208));
        gc.SetPenStyle(CGraphicsContext::ESolidPen);gc.SetPenColor(TRgb(230,240,255));
        gc.UseFont(iEikonEnv->LegendFont());
        gc.DrawText(_L("FRAMEBUFFER 0.1.38"),TPoint(2,13));
        TBuf<64> text;text.Format(_L("Map error %d"),iMapError);gc.DrawText(text,TPoint(2,26));
        const TText* labels[9]={_S("MODE"),_S("ADDR"),_S("OFFSET"),_S("PITCH"),_S("BPP"),
            _S("RGB"),_S("LAND"),_S("WIDTH"),_S("HEIGHT")};
        for(TInt n=0;n<9;++n){
            TPtrC label(labels[n]);
            if(n==1)text.Format(_L("%S %08x e%d"),&label,(TUint)iDisplayValues[n],iDisplayErrors[n]);
            else text.Format(_L("%S %d e%d"),&label,iDisplayValues[n],iDisplayErrors[n]);
            gc.DrawText(text,TPoint(2,39+n*12));
        }
        text.Format(_L("FBS %d ORI %d"),(TInt)iDsa->ScreenDevice()->DisplayMode(),(TInt)iDsa->ScreenDevice()->Orientation());
        gc.DrawText(text,TPoint(2,150));
        text.Format(_L("SV %d %08x"),iLegacyScreen.iScreenAddressValid,(TUint)iLegacyScreen.iScreenAddress);gc.DrawText(text,TPoint(2,164));
        text.Format(_L("SV SIZE %dx%d"),iLegacyScreen.iScreenSize.iWidth,iLegacyScreen.iScreenSize.iHeight);gc.DrawText(text,TPoint(2,178));
        TPoint origin=PositionRelativeToScreen();text.Format(_L("XY %d %d"),origin.iX,origin.iY);gc.DrawText(text,TPoint(2,190));
        gc.DrawText(_L("# / RIGHT SOFT: EXIT"),TPoint(2,201));gc.DiscardFont();
    }
    void PaintHud(CBitmapContext& gc) const {
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
    CFbsBitmap* iMenuBitmap;MenuState iMenu;Soft3D* iRenderer;Pixel* iPixels;
    CDirectScreenAccess* iDsa;TBool iDsaReady;TBool iDsaSuspended;
    Pixel* iScreenBase;TInt iScreenStride;TPoint iScreenOrigin;
    TInt iMapError,iDisplayValues[9],iDisplayErrors[9];TScreenInfoV01 iLegacyScreen;
    KeyRepeatGuard<RWsSession,TTimeIntervalMicroSeconds32> iRepeatGuard;FrameRateCounter iFpsCounter;FlightButtons iButtons;
    FlightModel iFlight;TInt iKeys;TBool iPaused;TInt iAccum;TInt iFps;TInt iRenderMs;TInt iPresentMs;TTime iLast;
};
// Own request pump, as in the N-Gage engine: WS input is not a CONE AO.
// The bootstrap idle callback only enters the loop once; it never ticks frames.
class CFlightUi:public CAknAppUi {
public:
    CFlightUi():iView(0),iStarter(0),iExitCallback(0),iQuit(EFalse),iOwnWs(EFalse),iAuxReady(EFalse){}
    void ConstructL(){
        BaseConstructL();SetKeyBlockMode(ENoKeyBlock);
        iView=new(ELeave)CFlightView;iView->ConstructL();
        AddToStackL(iView);iView->SetFocus(ETrue);
        iExitCallback=new(ELeave)CAsyncCallBack(TCallBack(Finish,this),CActive::EPriorityHigh);
        iStarter=CIdle::NewL(CActive::EPriorityIdle);
        iStarter->Start(TCallBack(Bootstrap,this));
    }
    ~CFlightUi(){
        StopRequests();delete iStarter;delete iExitCallback;
        if(iView){RemoveFromStack(iView);delete iView;}
    }
    void HandleCommandL(TInt cmd){
        if(cmd==EEikCmdExit||cmd==EAknSoftkeyExit){iQuit=ETrue;return;}
        if(cmd==0x7001&&iView)iView->ToggleMenu();
    }
    static TInt Bootstrap(TAny* self){static_cast<CFlightUi*>(self)->RunGameLoop();return 0;}
    static TInt Finish(TAny* self){static_cast<CFlightUi*>(self)->Exit();return 0;}
    void RunGameLoop(){
        TRAPD(error,OwnRequestsL());
        StopRequests();
        // Return out of the manual loop before deleting any UI/DSA objects.
        // The normal scheduler executes this callback after Bootstrap returns.
        if(error!=KErrNone){
            RFile log;
            if(log.Replace(iEikonEnv->FsSession(),_L("C:\\Skyliner700-loop.log"),EFileWrite)==KErrNone){
                TBuf8<64> line;line.Format(_L8("Manual request loop error: %d\r\n"),error);
                log.Write(line);log.Close();
            }
        }
        iExitCallback->CallBack();
    }
    void OwnRequestsL(){
        // Cancel synchronously: never have two EventReady consumers on a session.
        iEikonEnv->Cancel();
        iOwnWs=ETrue;
        iEikonEnv->WsSession().EventReady(&iEventStatus);
        while(!iQuit)PumpGameRequestsL(*this);
    }
    void Wait(){
        // Consume completed request signals without blocking an active flight.
        // Match the EKA1 pump pattern: RequestCount -> WaitForAnyRequest.
        RThread thread;
        TInt drained=0;
        while(drained<64&&thread.RequestCount()>0){User::WaitForAnyRequest();++drained;}
        if(!drained&&!iView->ContinuousFrames()&&!EventReady()&&!iAuxReady)
            User::WaitForAnyRequest();
    }
    TBool EventReady()const{return iEventStatus!=KRequestPending;}
    void ReadEventL(){
        User::LeaveIfError(iEventStatus.Int());
        TWsEvent event;iEikonEnv->WsSession().GetEvent(event);
        // Re-arm immediately, before dispatching callbacks that may leave.
        iEikonEnv->WsSession().EventReady(&iEventStatus);
        TInt type=event.Type();
        if(type==EEventKeyDown||type==EEventKeyUp||type==EEventKey){
            iView->HandleGameKey(*event.Key(),type);return;
        }
        // CONE's redraw AO and DSA AO are pumped below. Standard non-key events
        // retain S60 handling (focus, app messages, resource/screen changes).
        CAknAppUi::HandleWsEventL(event,reinterpret_cast<CCoeControl*>(event.Handle()));
    }
    void DispatchReadyL(){
        TInt error=KErrNone;
        // The N-Gage loop also services scheduler requests at priority -25.
        // Bound each batch so a busy auxiliary AO cannot starve the frame.
        iAuxReady=EFalse;
        for(TInt n=0;n<32&&!iQuit;++n){
            TBool ran=CActiveScheduler::RunIfReady(error,-25);
            User::LeaveIfError(error);if(!ran)break;
            if(n==31)iAuxReady=ETrue;
        }
    }
    TBool Quitting()const{return iQuit;}
    // Benchmark mode: no frame timer or target-FPS cap. Frame handles pause,
    // queued menu commands and deferred DSA repeat restoration on event wakes.
    TBool FrameReady()const{return ETrue;}
    void DrawFrameL(){iView->Frame();}
    void StopRequests(){
        if(iOwnWs){
            iEikonEnv->WsSession().EventReadyCancel();
            User::WaitForRequest(iEventStatus);iOwnWs=EFalse;
        }

    }
private:
    CFlightView* iView;CIdle* iStarter;CAsyncCallBack* iExitCallback;
    TRequestStatus iEventStatus;
    TBool iQuit,iOwnWs,iAuxReady;
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
