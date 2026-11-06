#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikapp.h>
#include <eikon.hrh>
#include <f32file.h>
#include <e32keys.h>
#include "owned_name.h"
const TUid KUid={0x0F700664};
class CCleanView:public CCoeControl {
    RFs iFs;TInt iDeleted,iFailed,iKept,iError;TBool iDone;
public:
    CCleanView():iDeleted(0),iFailed(0),iKept(0),iError(0),iDone(EFalse){}
    void ConstructL(){CreateWindowL();SetExtentToWholeScreen();User::LeaveIfError(iFs.Connect());ActivateL();}
    ~CCleanView(){iFs.Close();}
    void CleanL(){
        iDeleted=iFailed=iKept=iError=0;CDir* entries=0;
        TInt error=iFs.GetDir(_L("C:\\SystemDump\\*"),KEntryAttMaskSupported,ESortByName,entries);
        if(error==KErrPathNotFound||error==KErrNotFound){delete entries;iDone=ETrue;return;}
        User::LeaveIfError(error);CleanupStack::PushL(entries);
        for(TInt n=0;n<entries->Count();++n){const TEntry& entry=(*entries)[n];
            if(entry.IsDir()||!IsDumpOwned(entry.iName)){++iKept;continue;}
            TFileName path(_L("C:\\SystemDump\\"));path.Append(entry.iName);
            error=iFs.Delete(path);
            if(error==KErrNone)++iDeleted;else if(error!=KErrNotFound){++iFailed;iError=error;}
        }
        CleanupStack::PopAndDestroy(entries);
        // RmDir succeeds only when empty; no recursion or wildcard deletion.
        iFs.RmDir(_L("C:\\SystemDump\\"));iDone=ETrue;
    }
    void Clean(){TRAPD(error,CleanL());if(error!=KErrNone)iError=error;DrawNow();}
    TKeyResponse OfferKeyEventL(const TKeyEvent& key,TEventCode type){
        if(type!=EEventKey||key.iRepeats)return EKeyWasConsumed;
        if(key.iCode=='#'||key.iScanCode==EStdKeyDevice1)iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);
        else if(key.iCode=='5'||key.iScanCode==EStdKeyDevice0||key.iScanCode==EStdKeyDevice3)Clean();return EKeyWasConsumed;
    }
    void Draw(const TRect&)const{
        CWindowGc& gc=SystemGc();gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));gc.Clear(Rect());
        gc.SetPenColor(TRgb(220,239,250));gc.UseFont(iEikonEnv->LegendFont());gc.DrawText(_L("DUMP CLEANUP"),TPoint(3,18));
        gc.DrawText(_L("Close SystemDump first"),TPoint(3,40));gc.DrawText(_L("C:\\SystemDump\\ only"),TPoint(3,60));
        TBuf<60> line;line.Format(_L("Deleted %d failed %d"),iDeleted,iFailed);gc.DrawText(line,TPoint(3,90));
        line.Format(_L("Kept %d error %d"),iKept,iError);gc.DrawText(line,TPoint(3,110));
        if(iDone)gc.DrawText(_L("Cleanup finished"),TPoint(3,140));
        gc.DrawText(_L("5 CLEAN   # EXIT"),TPoint(3,185));gc.DiscardFont();
    }
};
class CCleanUi:public CAknAppUi {CCleanView* iView;public:CCleanUi():iView(0){}
    void ConstructL(){BaseConstructL();iView=new(ELeave)CCleanView;iView->ConstructL();AddToStackL(iView);iView->SetFocus(ETrue);}
    ~CCleanUi(){if(iView){RemoveFromStack(iView);delete iView;}}
    void HandleCommandL(TInt c){if(c==0x7601)iView->Clean();else if(c==EEikCmdExit||c==EAknSoftkeyExit)Exit();}
};
class CCleanDoc:public CAknDocument {public:CCleanDoc(CEikApplication& a):CAknDocument(a){}private:CEikAppUi* CreateAppUiL(){return new(ELeave)CCleanUi;}};
class CCleanApp:public CAknApplication {private:TUid AppDllUid()const{return KUid;}CApaDocument* CreateDocumentL(){return new(ELeave)CCleanDoc(*this);}};
EXPORT_C CApaApplication* NewApplication(){return new CCleanApp;}
GLDEF_C TInt E32Dll(TDllReason){return KErrNone;}
