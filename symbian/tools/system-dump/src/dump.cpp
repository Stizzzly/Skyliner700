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
#include "zip_store.h"
const TUid KDumpUid={0x0F700663};

class FileOutput {
    RFile file;unsigned position;TBuf8<4096> buffer;
public:
    FileOutput():position(0){}
    ~FileOutput(){Close();}
    void CreateL(RFs& fs,const TDesC& name){Close();User::LeaveIfError(file.Create(fs,name,EFileRead|EFileWrite|EFileShareExclusive));position=0;}
    void Flush(){if(buffer.Length()){User::LeaveIfError(file.Write(buffer));buffer.Zero();}}
    void Sync(){Flush();User::LeaveIfError(file.Flush());}
    void Write(const void* data,unsigned length){
        if(length>0x7fffffffu-position)User::Leave(KErrOverflow);
        const TUint8* p=(const TUint8*)data;
        while(length){
            if(!buffer.Length()&&length>=4096){User::LeaveIfError(file.Write(TPtrC8(p,length)));position+=length;return;}
            TInt n=Min((TInt)length,buffer.MaxLength()-buffer.Length());buffer.Append(p,n);
            p+=n;length-=n;position+=n;if(buffer.Length()==buffer.MaxLength())Flush();
        }
    }
    unsigned Position()const{return position;}
    void Truncate(unsigned size){Flush();User::LeaveIfError(file.SetSize(size));TInt p=size;User::LeaveIfError(file.Seek(ESeekStart,p));position=size;}
    void Rewind(){Flush();TInt p=0;User::LeaveIfError(file.Seek(ESeekStart,p));}
    unsigned Read(void* bytes,unsigned size){TPtr8 b((TUint8*)bytes,0,size);User::LeaveIfError(file.Read(b));return b.Length();}
    void Close(){file.Close();buffer.Zero();position=0;}
};

// ZIP uses UTF-8 names, including non-ASCII ROM resources; never lossy Copy().
static void ZipPathL(const TDesC& source,TDes8& out){
    out.Copy(_L8("Z/"));
    for(TInt i=3;i<source.Length();++i){
        TUint c=source[i];if(c=='\\')c='/';
        if(c>=0xd800&&c<=0xdbff){
            if(i+1>=source.Length()||source[i+1]<0xdc00||source[i+1]>0xdfff)User::Leave(KErrCorrupt);
            c=0x10000+((c-0xd800)<<10)+(source[++i]-0xdc00);
        }else if(c>=0xdc00&&c<=0xdfff)User::Leave(KErrCorrupt);
        if(c<0x80)out.Append((TUint8)c);
        else if(c<0x800){out.Append((TUint8)(0xc0|(c>>6)));out.Append((TUint8)(0x80|(c&63)));}
        else if(c<0x10000){out.Append((TUint8)(0xe0|(c>>12)));out.Append((TUint8)(0x80|((c>>6)&63)));out.Append((TUint8)(0x80|(c&63)));}
        else {out.Append((TUint8)(0xf0|(c>>18)));out.Append((TUint8)(0x80|((c>>12)&63)));out.Append((TUint8)(0x80|((c>>6)&63)));out.Append((TUint8)(0x80|(c&63)));}
    }
}

class CDumpView:public CCoeControl {
    enum Stage {Ready,Dumping,Complete,Cancelled,Failed};
    RFs iFs;RFile iInput,iReport;FileOutput iArchive,iCentral;
    ZipStore<FileOutput> iZip;CIdle* iWorker;CDesCArrayFlat* iDirs;CDir* iListing;
    TInt iDirectory,iEntry,iFiles,iSkipped,iError,iChunks;Stage iStage;
    TBool iInputOpen,iReportOpen,iOwnPart,iOwnCentral,iAutoSend,iSending;
    unsigned iRemaining;
    TFileName iCurrentDir,iCurrent,iPartPath,iCentralPath,iFinalPath,iReportPath;
    TBuf8<16384> iChunk;
public:
    CDumpView():iZip(iArchive,iCentral),iWorker(0),iDirs(0),iListing(0),
        iDirectory(0),iEntry(0),iFiles(0),iSkipped(0),iError(0),iChunks(0),iStage(Ready),
        iInputOpen(EFalse),iReportOpen(EFalse),iOwnPart(EFalse),iOwnCentral(EFalse),iAutoSend(EFalse),iSending(EFalse),iRemaining(0){}
    void ConstructL(){CreateWindowL();SetExtentToWholeScreen();User::LeaveIfError(iFs.Connect());
        iDirs=new(ELeave)CDesCArrayFlat(32);iWorker=CIdle::NewL(CActive::EPriorityIdle);ActivateL();}
    ~CDumpView(){if(iWorker)iWorker->Cancel();CleanupJob();delete iWorker;delete iDirs;delete iListing;iFs.Close();}
    void LogL(const TDesC& path,TInt error){
        if(!iReportOpen)return;
        TBuf8<1024> name;ZipPathL(path,name);TBuf8<80> prefix;prefix.Format(_L8("%d "),error);
        User::LeaveIfError(iReport.Write(prefix));User::LeaveIfError(iReport.Write(name));User::LeaveIfError(iReport.Write(_L8("\r\n")));
    }
    void SkipL(const TDesC& path,TInt error){++iSkipped;LogL(path,error);}
    void BeginEntryL(const TDesC& path,unsigned size,TBool directory,const TTime& modified){
        TBuf8<1024> name;ZipPathL(path,name);TDateTime date=modified.DateTime();
        unsigned dosDate=((Max(1980,Min(2107,date.Year()))-1980)<<9)|((date.Month()+1)<<5)|(date.Day()+1);
        unsigned dosTime=(date.Hour()<<11)|(date.Minute()<<5)|(date.Second()>>1);
        if(!iZip.Begin((const char*)name.Ptr(),name.Length(),size,directory,dosTime,dosDate))User::Leave(KErrOverflow);
    }
    void StartL(){
        if(iStage==Dumping)return;
        CleanupJob();delete iListing;iListing=0;iDirs->Reset();
        iFiles=iSkipped=iError=iChunks=iDirectory=iEntry=0;
        // A new ZIP writer is required for each retry. Placement new is avoided:
        // failed/cancelled jobs are restarted by closing and reopening the app.
        if(iStage!=Ready)User::Leave(KErrNotReady);
        TInt error=iFs.MkDirAll(_L("E:\\SystemDump\\"));if(error!=KErrNone&&error!=KErrAlreadyExists)User::Leave(error);
        TTime now;now.HomeTime();TDateTime d=now.DateTime();TFileName stem;
        stem.Format(_L("E:\\SystemDump\\N6600_%04d%02d%02d_%02d%02d%02d"),d.Year(),d.Month()+1,d.Day()+1,d.Hour(),d.Minute(),d.Second());
        iPartPath=stem;iPartPath.Append(_L(".part"));iCentralPath=stem;iCentralPath.Append(_L(".central"));
        iFinalPath=stem;iFinalPath.Append(_L(".zip"));iReportPath=stem;iReportPath.Append(_L(".log"));
        // Create rather than Replace: existing dumps are never overwritten.
        User::LeaveIfError(iReport.Create(iFs,iReportPath,EFileWrite|EFileShareAny));iReportOpen=ETrue;
        User::LeaveIfError(iReport.Write(_L8("SystemDump 0.1.0; source Z:\\ only; file-level ROM extraction, not a raw ROM image.\r\n")));
        iArchive.CreateL(iFs,iPartPath);iOwnPart=ETrue;iCentral.CreateL(iFs,iCentralPath);iOwnCentral=ETrue;
        iDirs->AppendL(_L("Z:\\"));BeginEntryL(_L("Z:\\"),0,ETrue,now);if(!iZip.End())User::Leave(KErrCorrupt);
        iStage=Dumping;DrawNow();iWorker->Start(TCallBack(Work,this));
    }
    void Start(){if(iSending)return;if(iStage==Complete){Send();return;}TRAPD(error,StartL());if(error!=KErrNone){iError=error;if(iStage!=Complete){iStage=Failed;CleanupJob();}DrawNow();}}
    static TInt Work(TAny* p){return static_cast<CDumpView*>(p)->Step();}
    TInt Step(){
        TRAPD(error,StepL());
        if(error!=KErrNone){iError=error;iStage=Failed;CleanupJob();DrawNow();return 0;}
        if(iStage==Dumping){if((++iChunks&31)==0)DrawNow();return 1;}
        DrawNow();
        if(iAutoSend){iAutoSend=EFalse;Send();}
        return 0;
    }
    void StepL(){
        if(iInputOpen){
            TInt error=iInput.Read(iChunk,Min((TInt)iRemaining,iChunk.MaxLength()));
            if(error!=KErrNone||(!iChunk.Length()&&iRemaining)){
                iInput.Close();iInputOpen=EFalse;iZip.Abort();SkipL(iCurrent,error==KErrNone?KErrCorrupt:error);return;
            }
            if(!iZip.Data(iChunk.Ptr(),iChunk.Length()))User::Leave(KErrCorrupt);
            iRemaining-=iChunk.Length();
            if(!iRemaining){iInput.Close();iInputOpen=EFalse;if(!iZip.End())User::Leave(KErrCorrupt);++iFiles;LogL(iCurrent,KErrNone);}
            return;
        }
        if(iListing&&iEntry<iListing->Count()){
            const TEntry& entry=(*iListing)[iEntry++];
            if(entry.iName==_L(".")||entry.iName==_L(".."))return;
            if(iCurrentDir.Length()+entry.iName.Length()+(entry.IsDir()?1:0)>iCurrent.MaxLength()){SkipL(iCurrentDir,KErrOverflow);return;}
            iCurrent=iCurrentDir;iCurrent.Append(entry.iName);
            if(entry.IsDir()){
                iCurrent.Append('\\');iDirs->AppendL(iCurrent);
                BeginEntryL(iCurrent,0,ETrue,entry.iModified);if(!iZip.End())User::Leave(KErrCorrupt);return;
            }
            TInt error=iInput.Open(iFs,iCurrent,EFileRead|EFileShareReadersOnly);
            if(error!=KErrNone){SkipL(iCurrent,error);return;}
            iInputOpen=ETrue;TInt size=0;error=iInput.Size(size);
            if(error!=KErrNone||size<0){iInput.Close();iInputOpen=EFalse;SkipL(iCurrent,error==KErrNone?KErrCorrupt:error);return;}
            iRemaining=size;BeginEntryL(iCurrent,iRemaining,EFalse,entry.iModified);
            if(!iRemaining){iInput.Close();iInputOpen=EFalse;if(!iZip.End())User::Leave(KErrCorrupt);++iFiles;LogL(iCurrent,KErrNone);}
            return;
        }
        delete iListing;iListing=0;iEntry=0;
        if(iDirectory<iDirs->Count()){
            iCurrentDir=(*iDirs)[iDirectory++];
            if(iCurrentDir.Length()>=iCurrentDir.MaxLength()){SkipL(iCurrentDir,KErrOverflow);return;}
            TFileName mask(iCurrentDir);mask.Append('*');
            TInt error=iFs.GetDir(mask,KEntryAttMaskSupported,ESortByName,iListing);
            if(error!=KErrNone){delete iListing;iListing=0;SkipL(iCurrentDir,error);}return;
        }
        if(!iZip.Finish())User::Leave(KErrCorrupt);
        iArchive.Sync();iArchive.Close();iCentral.Close();
        User::LeaveIfError(iFs.Rename(iPartPath,iFinalPath));iOwnPart=EFalse;
        iFs.Delete(iCentralPath);iOwnCentral=EFalse;
        TBuf8<100> summary;summary.Format(_L8("END files %d skipped %d zip entries %u\r\n"),iFiles,iSkipped,iZip.Count());
        User::LeaveIfError(iReport.Write(summary));User::LeaveIfError(iReport.Flush());iReport.Close();iReportOpen=EFalse;
        iStage=Complete;iAutoSend=ETrue;
    }
    void CleanupJob(){
        iInput.Close();iInputOpen=EFalse;iArchive.Close();iCentral.Close();iReport.Close();iReportOpen=EFalse;
        if(iOwnPart){iFs.Delete(iPartPath);iOwnPart=EFalse;}
        if(iOwnCentral){iFs.Delete(iCentralPath);iOwnCentral=EFalse;}
    }
    void Cancel(){if(iStage==Dumping){iWorker->Cancel();iStage=Cancelled;CleanupJob();DrawNow();}}
    void SendL(){
        if(iStage!=Complete)User::Leave(KErrNotReady);
        CDesCArrayFlat* attachments=new(ELeave)CDesCArrayFlat(2);CleanupStack::PushL(attachments);
        attachments->AppendL(iFinalPath);attachments->AppendL(iReportPath);
        CSendAppUi* send=CSendAppUi::NewLC(0x7500);
        send->CreateAndSendMessageL(KSenduiMtmBtUid,0,attachments);
        CleanupStack::PopAndDestroy(send);CleanupStack::PopAndDestroy(attachments);
    }
    TBool Sending()const{return iSending;}
    void Send(){if(iSending)return;iSending=ETrue;TRAPD(error,SendL());iSending=EFalse;if(error!=KErrNone&&error!=KErrCancel)iError=error;DrawNow();}
    TKeyResponse OfferKeyEventL(const TKeyEvent& key,TEventCode type){
        if(type!=EEventKey||key.iRepeats>0)return EKeyWasConsumed;
        if(key.iScanCode==EStdKeyDevice1||key.iCode=='#'){iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return EKeyWasConsumed;}
        if(key.iCode=='0')Cancel();
        else if(key.iCode=='7')Send();
        else if(key.iCode=='5'||key.iScanCode==EStdKeyDevice0||key.iScanCode==EStdKeyDevice3)Start();
        return EKeyWasConsumed;
    }
    void Draw(const TRect&)const{
        CWindowGc& gc=SystemGc();gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));gc.Clear(Rect());
        gc.SetPenColor(TRgb(220,239,250));gc.UseFont(iEikonEnv->LegendFont());
        gc.DrawText(_L("SYSTEM DUMP 0.1.0"),TPoint(3,16));
        const TText* stages[]={_S("5 START: Z drive only"),_S("Reading system files"),_S("ZIP READY - 7 SEND"),_S("Cancelled - reopen app"),_S("Failed - reopen app")};
        gc.DrawText(TPtrC(stages[iStage]),TPoint(3,37));
        TBuf<80> line;line.Format(_L("Files %d  skipped %d"),iFiles,iSkipped);gc.DrawText(line,TPoint(3,57));
        line.Format(_L("ZIP entries %u"),iZip.Count());gc.DrawText(line,TPoint(3,75));
        line.Format(_L("Error %d"),iError);gc.DrawText(line,TPoint(3,93));
        gc.DrawText(iCurrent.Right(Min(25,iCurrent.Length())),TPoint(3,113));
        gc.DrawText(_L("E:\\SystemDump\\"),TPoint(3,135));
        gc.DrawText(_L("5 START / SEND ZIP"),TPoint(3,160));gc.DrawText(_L("0 CANCEL   7 RESEND"),TPoint(3,180));
        gc.DrawText(_L("# / RIGHT SOFT: EXIT"),TPoint(3,200));gc.DiscardFont();
    }
};
class CDumpUi:public CAknAppUi {
    CDumpView* iView;
public:
    CDumpUi():iView(0){}void ConstructL(){BaseConstructL();iView=new(ELeave)CDumpView;iView->ConstructL();AddToStackL(iView);iView->SetFocus(ETrue);}
    ~CDumpUi(){if(iView){RemoveFromStack(iView);delete iView;}}
    void HandleCommandL(TInt command){if(command==0x7401){iView->Start();return;}if(command==EEikCmdExit||command==EAknSoftkeyExit){if(iView->Sending())return;iView->Cancel();Exit();}}
};
class CDumpDocument:public CAknDocument {public:CDumpDocument(CEikApplication& a):CAknDocument(a){}private:CEikAppUi* CreateAppUiL(){return new(ELeave)CDumpUi;}};
class CDumpApplication:public CAknApplication {private:TUid AppDllUid()const{return KDumpUid;}CApaDocument* CreateDocumentL(){return new(ELeave)CDumpDocument(*this);}};
EXPORT_C CApaApplication* NewApplication(){return new CDumpApplication;}
GLDEF_C TInt E32Dll(TDllReason){return KErrNone;}
