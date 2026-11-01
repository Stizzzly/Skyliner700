#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikapp.h>
#include <eikon.hrh>
#include <f32file.h>
#include <badesca.h>
#include <e32keys.h>
#include <sendui.h>
#include <senduimtmuids.h>
const TUid KInventoryUid={0x0F700662};
class CInventoryView:public CCoeControl {
public:
    CInventoryView():iLines(0),iTop(0),iLogOpen(EFalse){}
    void ConstructL(){
        CreateWindowL();SetExtentToWholeScreen();
        iLines=new(ELeave)CDesCArrayFlat(16);User::LeaveIfError(iFs.Connect());
        ActivateL();Scan();
    }
    ~CInventoryView(){iLog.Close();iFs.Close();delete iLines;}
    void AddL(const TDesC& line){
        if(iLogOpen){TBuf8<600> bytes;bytes.Copy(line.Left(580));bytes.Append(_L8("\r\n"));iLog.Write(bytes);}
        if(!line.Length()){iLines->AppendL(_L(" "));return;}
        for(TInt start=0;start<line.Length();start+=24)iLines->AppendL(line.Mid(start,Min(24,line.Length()-start)));
    }
    void StatusL(const TDesC& label,TInt error){TBuf<100> text;text.Format(_L("%S: %d"),&label,error);AddL(text);}
    template<class Finder> void ListObjectsL(const TDesC& label){
        AddL(label);Finder find(_L("*"));TFullName name;TInt error,count=0;
        while((error=find.Next(name))==KErrNone){AddL(name);if(++count>=512){AddL(_L("LIMIT 512"));break;}}
        StatusL(_L("Count"),count);StatusL(_L("Next status"),error);AddL(_L(" "));
    }
    void ListFilesL(const TDesC& mask){
        AddL(mask);CDir* entries=0;TInt error=iFs.GetDir(mask,KEntryAttMaskSupported,ESortByName,entries);
        if(error!=KErrNone){StatusL(_L("GetDir"),error);delete entries;return;}
        CleanupStack::PushL(entries);
        for(TInt i=0;i<entries->Count();++i)AddL((*entries)[i].iName);
        StatusL(_L("Count"),entries->Count());CleanupStack::PopAndDestroy(entries);
    }
    void ScanL(){
        iLines->Reset();iTop=0;iLog.Close();iLogOpen=EFalse;
        TInt error=iLog.Replace(iFs,_L("C:\\DeviceInventory.log"),EFileWrite|EFileShareAny);
        iLogOpen=(error==KErrNone);
        AddL(_L("DEVICE INVENTORY 0.1.1"));StatusL(_L("Log open"),error);
        ListObjectsL<TFindLogicalDevice>(_L("LOGICAL DEVICES"));
        ListObjectsL<TFindPhysicalDevice>(_L("PHYSICAL DEVICES"));
        ListObjectsL<TFindLogicalChannel>(_L("LOGICAL CHANNELS"));
        ListFilesL(_L("Z:\\system\\libs\\*.ldd"));ListFilesL(_L("Z:\\system\\libs\\*.pdd"));
        ListFilesL(_L("Z:\\system\\libs\\*key*"));ListFilesL(_L("Z:\\system\\libs\\*kbd*"));
        ListFilesL(_L("Z:\\system\\libs\\*kdata*"));
        AddL(_L("END - enumeration only"));if(iLogOpen)iLog.Flush();
    }
    void Scan(){TRAPD(error,ScanL());if(error!=KErrNone){TBuf<40> message;message.Format(_L("Scan leave %d"),error);TRAPD(logError,AddL(message));(void)logError;}DrawNow();}
    void SendFilesL(TBool drivers){
        CDesCArrayFlat* attachments=new(ELeave)CDesCArrayFlat(4);CleanupStack::PushL(attachments);
        if(drivers){
            TInt error=iFs.MkDirAll(_L("C:\\DriverStudy\\"));
            if(error!=KErrNone&&error!=KErrAlreadyExists)User::Leave(error);
            CFileMan* copy=CFileMan::NewL(iFs);CleanupStack::PushL(copy);
            const TPtrC sources[]={_L("Z:\\system\\libs\\dhal.ldd"),_L("Z:\\system\\libs\\helper.ldd")};
            const TPtrC targets[]={_L("C:\\DriverStudy\\dhal.bin"),_L("C:\\DriverStudy\\helper.bin")};
            for(TInt i=0;i<2;++i){
                iFs.SetAtt(targets[i],0,KEntryAttReadOnly);
                error=copy->Copy(sources[i],targets[i]);StatusL(sources[i],error);
                if(error==KErrNone)attachments->AppendL(targets[i]);
            }
            CleanupStack::PopAndDestroy(copy);
        }
        if(iLogOpen){iLog.Flush();iLog.Close();iLogOpen=EFalse;}
        TEntry entry;if(iFs.Entry(_L("C:\\DeviceInventory.log"),entry)==KErrNone)attachments->AppendL(_L("C:\\DeviceInventory.log"));
        if(!attachments->Count())User::Leave(KErrNotFound);
        CSendAppUi* send=CSendAppUi::NewLC(0x7300);
        // Bluetooth-only UI. The user selects the receiving computer.
        send->CreateAndSendMessageL(KSenduiMtmBtUid,0,attachments);
        CleanupStack::PopAndDestroy(send);CleanupStack::PopAndDestroy(attachments);
    }
    void SendFiles(TBool drivers){
        TRAPD(error,SendFilesL(drivers));
        if(error!=KErrNone&&error!=KErrCancel){TRAPD(logError,StatusL(_L("Send error"),error));(void)logError;}
        DrawNow();
    }
    TKeyResponse OfferKeyEventL(const TKeyEvent& key,TEventCode type){
        if(type!=EEventKey||key.iRepeats>0)return EKeyWasConsumed;
        if(key.iScanCode==EStdKeyDevice1||key.iCode=='#'||key.iCode==EKeyEscape){iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return EKeyWasConsumed;}
        if(key.iCode==EKeyDownArrow||key.iCode=='8'){iTop=Min(iTop+8,Max(0,iLines->Count()-8));DrawNow();}
        else if(key.iCode==EKeyUpArrow||key.iCode=='2'){iTop=Max(0,iTop-8);DrawNow();}
        else if(key.iCode=='7'||key.iScanCode==EStdKeyDevice0)SendFiles(EFalse);
        else if(key.iCode=='9')SendFiles(ETrue);
        else if(key.iCode=='5'||key.iScanCode==EStdKeyDevice3)Scan();
        return EKeyWasConsumed;
    }
private:
    void Draw(const TRect&)const{
        CWindowGc& gc=SystemGc();gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));gc.Clear(Rect());
        gc.SetPenColor(TRgb(220,239,250));gc.UseFont(iEikonEnv->LegendFont());
        TBuf<40> title;title.Format(_L("DEVICE INVENTORY %d/%d"),iTop+1,iLines?iLines->Count():0);gc.DrawText(title,TPoint(3,15));
        if(iLines)for(TInt i=0;i<8&&iTop+i<iLines->Count();++i)gc.DrawText((*iLines)[iTop+i],TPoint(3,35+i*16));
        gc.DrawText(_L("7 LOG  9 DRIVER FILES"),TPoint(3,177));gc.DrawText(_L("5 SCAN; UP/DOWN pages"),TPoint(3,194));gc.DiscardFont();
    }
    CDesCArrayFlat* iLines;TInt iTop;RFs iFs;RFile iLog;TBool iLogOpen;
};
class CInventoryUi:public CAknAppUi {
public:CInventoryUi():iView(0){}void ConstructL(){BaseConstructL();iView=new(ELeave)CInventoryView;iView->ConstructL();AddToStackL(iView);iView->SetFocus(ETrue);}
    ~CInventoryUi(){if(iView){RemoveFromStack(iView);delete iView;}}
    void HandleCommandL(TInt command){if(command==0x7201){iView->SendFiles(EFalse);return;}if(command==EEikCmdExit||command==EAknSoftkeyExit)Exit();}
private:CInventoryView* iView;
};
class CInventoryDocument:public CAknDocument {public:CInventoryDocument(CEikApplication& a):CAknDocument(a){}private:CEikAppUi* CreateAppUiL(){return new(ELeave)CInventoryUi;}};
class CInventoryApplication:public CAknApplication {private:TUid AppDllUid()const{return KInventoryUid;}CApaDocument* CreateDocumentL(){return new(ELeave)CInventoryDocument(*this);}};
EXPORT_C CApaApplication* NewApplication(){return new CInventoryApplication;}
GLDEF_C TInt E32Dll(TDllReason){return KErrNone;}
