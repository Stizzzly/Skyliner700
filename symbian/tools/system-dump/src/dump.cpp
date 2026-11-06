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
#include "direct_obex.h"
#include "crc_table.h"
const TUid KDumpUid={0x0F700663};
static TInt Hex(TUint c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;User::Leave(KErrCorrupt);return 0;}
static void EncodePath(const TDesC& path,TDes8& out){
    const TUint8 digits[]="0123456789abcdef";out.Zero();
    for(TInt i=0;i<path.Length();++i){TUint c=path[i];out.Append(digits[(c>>4)&15]);out.Append(digits[c&15]);out.Append(digits[(c>>12)&15]);out.Append(digits[(c>>8)&15]);}
}
static void DecodePathL(const TDesC8& bytes,TDes& path){
    if((bytes.Length()&3)||bytes.Length()/4>path.MaxLength())User::Leave(KErrCorrupt);path.Zero();
    for(TInt i=0;i<bytes.Length();i+=4){TUint c=(Hex(bytes[i])<<4)|Hex(bytes[i+1])|(Hex(bytes[i+2])<<12)|(Hex(bytes[i+3])<<8);if(!c)User::Leave(KErrCorrupt);path.Append(TChar(c));}
    if(path.Length()<3||path.Left(3).CompareF(_L("Z:\\"))||path.Find(_L("\\..\\"))!=KErrNotFound)User::Leave(KErrArgument);
}
class CDumpView:public CCoeControl,public MTransfer {
    enum Stage {Ready,Indexing,Prepared,Transferring,Complete,Cancelled,Failed};
    RFs iFs;RFile iInput,iManifest,iReport,iIndex;
    CIdle* iWorker;CDirectObex* iBt;CDesCArrayFlat* iDirs;CDir* iListing;
    Stage iStage;TInt iDirectory,iEntry,iFiles,iSkipped,iSent,iError,iPhase,iUpdates,iReadPos,iRole;
    TBool iInputOpen,iManifestOpen,iReportOpen,iIndexOpen;
    unsigned iRemaining,iFileSize,iCrc;
    TFileName iCurrentDir,iCurrent,iStem,iManifestPart,iManifestPath,iReportPath,iSendName;
    TBuf8<16384> iChunk;TBuf8<4096> iReadBuffer;TBuf8<1600> iLine;
public:
    CDumpView():iWorker(0),iBt(0),iDirs(0),iListing(0),iStage(Ready),iDirectory(0),iEntry(0),
        iFiles(0),iSkipped(0),iSent(0),iError(0),iPhase(0),iUpdates(0),iReadPos(0),iRole(0),
        iInputOpen(EFalse),iManifestOpen(EFalse),iReportOpen(EFalse),iIndexOpen(EFalse){}
    void ConstructL(){CreateWindowL();SetExtentToWholeScreen();User::LeaveIfError(iFs.Connect());
        iDirs=new(ELeave)CDesCArrayFlat(32);iWorker=CIdle::NewL(CActive::EPriorityIdle);iBt=new(ELeave)CDirectObex(*this);ActivateL();}
    ~CDumpView(){if(iWorker)iWorker->Cancel();delete iBt;CloseFiles();delete iWorker;delete iDirs;delete iListing;iFs.Close();}
    void CloseFiles(){iInput.Close();iManifest.Close();iReport.Close();iIndex.Close();
        iInputOpen=iManifestOpen=iReportOpen=iIndexOpen=EFalse;}
    void RecordL(char kind,const TDesC& path,TInt size,unsigned crc,const TDesC& name){
        TBuf8<1100> hex;EncodePath(path,hex);TBuf8<1600> row;row.Append(kind);row.Append('\t');
        TBuf8<80> ascii;ascii.Copy(name);row.Append(ascii);row.Append('\t');row.AppendNum(size);row.Append('\t');
        row.AppendNumFixedWidth(crc,EHex,8);row.Append('\t');row.Append(hex);row.Append(_L8("\r\n"));
        User::LeaveIfError(iManifest.Write(row));
    }
    void SkipL(const TDesC& path,TInt error){++iSkipped;RecordL('E',path,error,0,_L("-"));}
    void StartL(){
        if(iStage==Indexing||iStage==Transferring)return;
        if(iStage==Prepared||iStage==Complete){SendL();return;}
        if(iStage!=Ready)User::Leave(KErrNotReady);
        TInt error=iFs.MkDirAll(_L("C:\\SystemDump\\"));if(error!=KErrNone&&error!=KErrAlreadyExists)User::Leave(error);
        TTime now;now.HomeTime();TDateTime d=now.DateTime();
        iStem.Format(_L("N6600_%04d%02d%02d_%02d%02d%02d"),d.Year(),d.Month()+1,d.Day()+1,d.Hour(),d.Minute(),d.Second());
        iManifestPath.Copy(_L("C:\\SystemDump\\"));iManifestPath.Append(iStem);iManifestPath.Append(_L("_manifest.tsv"));
        iManifestPart=iManifestPath;iManifestPart.Append(_L(".part"));
        iReportPath.Copy(_L("C:\\SystemDump\\"));iReportPath.Append(iStem);iReportPath.Append(_L("_transfer.log"));
        User::LeaveIfError(iManifest.Create(iFs,iManifestPart,EFileWrite|EFileShareAny));iManifestOpen=ETrue;
        User::LeaveIfError(iReport.Create(iFs,iReportPath,EFileWrite|EFileShareAny));iReportOpen=ETrue;
        User::LeaveIfError(iManifest.Write(_L8("# SystemDump 0.2.0; F name size crc32 path_utf16le_hex; D directory; E read error\r\n")));
        iDirs->AppendL(_L("Z:\\"));RecordL('D',_L("Z:\\"),0,0,_L("-"));
        iStage=Indexing;DrawNow();iWorker->Start(TCallBack(Work,this));
    }
    void Start(){TRAPD(error,StartL());if(error!=KErrNone)Failure(error);}
    void Failure(TInt error){iError=error;iStage=Failed;DrawNow();}
    static TInt Work(TAny* self){return static_cast<CDumpView*>(self)->Step();}
    TInt Step(){
        TRAPD(error,StepL());if(error!=KErrNone){CloseFiles();Failure(error);return 0;}
        if(iStage==Indexing){if((++iUpdates&31)==0)DrawNow();return 1;}
        DrawNow();TRAPD(sendError,SendL());if(sendError!=KErrNone)Failure(sendError);return 0;
    }
    void FinishInputL(){
        iInput.Close();iInputOpen=EFalse;
        if(iFiles>=999999)User::Leave(KErrOverflow);
        iSendName=iStem;TBuf<20> suffix;suffix.Format(_L("_%06d.bin"),iFiles+1);iSendName.Append(suffix);
        RecordL('F',iCurrent,iFileSize,iCrc^0xffffffffu,iSendName);++iFiles;
    }
    void StepL(){
        if(iInputOpen){
            TInt error=iInput.Read(iChunk,Min((TInt)iRemaining,iChunk.MaxLength()));
            if(error!=KErrNone||(!iChunk.Length()&&iRemaining)){iInput.Close();iInputOpen=EFalse;SkipL(iCurrent,error==KErrNone?KErrCorrupt:error);return;}
            for(TInt n=0;n<iChunk.Length();++n)iCrc=ZipCrcTable[(iCrc^iChunk[n])&255]^(iCrc>>8);
            iRemaining-=iChunk.Length();if(!iRemaining)FinishInputL();return;
        }
        if(iListing&&iEntry<iListing->Count()){
            const TEntry& entry=(*iListing)[iEntry++];if(entry.iName==_L(".")||entry.iName==_L(".."))return;
            if(iCurrentDir.Length()+entry.iName.Length()+(entry.IsDir()?1:0)>iCurrent.MaxLength()){SkipL(iCurrentDir,KErrOverflow);return;}
            iCurrent=iCurrentDir;iCurrent.Append(entry.iName);
            if(entry.IsDir()){iCurrent.Append('\\');iDirs->AppendL(iCurrent);RecordL('D',iCurrent,0,0,_L("-"));return;}
            TInt error=iInput.Open(iFs,iCurrent,EFileRead|EFileShareReadersOnly);if(error!=KErrNone){SkipL(iCurrent,error);return;}
            iInputOpen=ETrue;TInt size;error=iInput.Size(size);
            if(error!=KErrNone||size<0){iInput.Close();iInputOpen=EFalse;SkipL(iCurrent,error==KErrNone?KErrCorrupt:error);return;}
            iFileSize=iRemaining=size;iCrc=0xffffffffu;if(!size)FinishInputL();return;
        }
        delete iListing;iListing=0;iEntry=0;
        if(iDirectory<iDirs->Count()){
            iCurrentDir=(*iDirs)[iDirectory++];
            if(iCurrentDir.Length()>=iCurrentDir.MaxLength()){SkipL(iCurrentDir,KErrOverflow);return;}
            TFileName mask(iCurrentDir);mask.Append('*');TInt error=iFs.GetDir(mask,KEntryAttMaskSupported,ESortByName,iListing);
            if(error!=KErrNone){delete iListing;iListing=0;SkipL(iCurrentDir,error);}return;
        }
        TBuf8<100> summary;summary.Format(_L8("# END files %d skipped %d\r\n"),iFiles,iSkipped);
        User::LeaveIfError(iManifest.Write(summary));User::LeaveIfError(iManifest.Flush());iManifest.Close();iManifestOpen=EFalse;
        User::LeaveIfError(iFs.Rename(iManifestPart,iManifestPath));iStage=Prepared;
    }
    void SendL(){
        if(iStage!=Prepared&&iStage!=Complete&&iStage!=Failed)User::Leave(KErrNotReady);
        TEntry manifest;User::LeaveIfError(iFs.Entry(iManifestPath,manifest));
        iBt->Stop(); // Release an old OBEX file handle before appending the report.
        if(!iReportOpen){
            User::LeaveIfError(iReport.Open(iFs,iReportPath,EFileWrite|EFileShareAny));iReportOpen=ETrue;
            TInt end=0;User::LeaveIfError(iReport.Seek(ESeekEnd,end));
        }
        iSent=0;iRole=0;iError=0;iIndex.Close();iIndexOpen=EFalse;iReadBuffer.Zero();iReadPos=0;
        iStage=Transferring;iBt->StartL();
    }
    void Send(){if(iStage==Indexing||iStage==Transferring)return;if(iStage==Ready){Start();return;}TRAPD(error,SendL());if(error!=KErrNone)Failure(error);}
    TBool ReadLineL(){
        iLine.Zero();
        for(;;){
            if(iReadPos==iReadBuffer.Length()){
                User::LeaveIfError(iIndex.Read(iReadBuffer));iReadPos=0;if(!iReadBuffer.Length())return iLine.Length()!=0;
            }
            TUint8 c=iReadBuffer[iReadPos++];if(c=='\n')return ETrue;if(c=='\r')continue;
            if(iLine.Length()==iLine.MaxLength())User::Leave(KErrOverflow);iLine.Append(c);
        }
    }
    TBool NextObjectL(TFileName& source,TFileName& name){
        if(iRole==0){
            source=iManifestPath;TParse parse;User::LeaveIfError(parse.Set(source,0,0));name=parse.NameAndExt();
            // OBEX must be the only owner while sending this same file.
            // Open the manifest reader only after its PUT completes and the
            // old CObexFileObject has been deleted by PutNextL.
            iRole=1;return ETrue;
        }
        if(iRole==1){
            if(!iIndexOpen){User::LeaveIfError(iIndex.Open(iFs,iManifestPath,EFileRead|EFileShareReadersOnly));iIndexOpen=ETrue;}
            while(ReadLineL()){
                if(iLine.Length()<2||iLine[0]!='F'||iLine[1]!='\t')continue;
                TPtrC8 rest=iLine.Mid(2);TInt split=rest.Locate('\t');if(split<1)User::Leave(KErrCorrupt);
                name.Copy(rest.Left(split));rest.Set(rest.Mid(split+1));
                for(TInt n=0;n<2;++n){split=rest.Locate('\t');if(split<0)User::Leave(KErrCorrupt);rest.Set(rest.Mid(split+1));}
                DecodePathL(rest,source);iCurrent=source;return ETrue;
            }
            iIndex.Close();iIndexOpen=EFalse;iRole=2;
        }
        if(iRole==2){
            TBuf8<80> summary;summary.Format(_L8("sent %d indexed %d skipped %d\r\n"),iSent,iFiles,iSkipped);
            User::LeaveIfError(iReport.Write(summary));User::LeaveIfError(iReport.Flush());iReport.Close();iReportOpen=EFalse;
            source=iReportPath;TParse parse;User::LeaveIfError(parse.Set(source,0,0));name=parse.NameAndExt();iRole=3;return ETrue;
        }
        return EFalse;
    }
    void ObjectSentL(const TDesC& name){
        if(name.Right(Min(4,name.Length()))==_L(".bin"))++iSent;
        if(iReportOpen){TBuf8<128> line;line.Copy(name);line.Append(_L8(" OK\r\n"));User::LeaveIfError(iReport.Write(line));}
        DrawNow();
    }
    void TransferState(TInt phase,TInt error){
        iPhase=phase;if(error!=KErrNone){iError=error;iStage=Failed;}
        else if(phase==CDirectObex::Done)iStage=Complete;DrawNow();
    }
    void Cancel(){if(iWorker)iWorker->Cancel();if(iBt)iBt->Stop();CloseFiles();if(iStage!=Complete)iStage=Cancelled;DrawNow();}
    TKeyResponse OfferKeyEventL(const TKeyEvent& key,TEventCode type){
        if(type!=EEventKey||key.iRepeats>0)return EKeyWasConsumed;
        if(key.iCode=='#'||key.iScanCode==EStdKeyDevice1){iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit);return EKeyWasConsumed;}
        if(key.iCode=='0')Cancel();else if(key.iCode=='7')Send();
        else if(key.iCode=='5'||key.iScanCode==EStdKeyDevice3||key.iScanCode==EStdKeyDevice0)Start();return EKeyWasConsumed;
    }
    void Draw(const TRect&)const{
        CWindowGc& gc=SystemGc();gc.SetBrushStyle(CGraphicsContext::ESolidBrush);gc.SetBrushColor(TRgb(7,20,34));gc.Clear(Rect());
        gc.SetPenColor(TRgb(220,239,250));gc.UseFont(iEikonEnv->LegendFont());gc.DrawText(_L("SYSTEM DUMP 0.2.1"),TPoint(3,16));
        const TText* states[]={_S("5 START - NO MMC"),_S("Index + CRC32 of Z:"),_S("7 SEND TO COMPUTER"),_S("Direct Bluetooth OBEX"),_S("TRANSFER COMPLETE"),_S("Cancelled - reopen"),_S("Failed - reopen")};
        gc.DrawText(TPtrC(states[iStage]),TPoint(3,38));TBuf<80> line;
        line.Format(_L("Files %d skipped %d"),iFiles,iSkipped);gc.DrawText(line,TPoint(3,60));
        line.Format(_L("Sent %d / %d"),iSent,iFiles);gc.DrawText(line,TPoint(3,80));
        line.Format(_L("Phase %d error %d"),iPhase,iError);gc.DrawText(line,TPoint(3,100));
        gc.DrawText(iCurrent.Right(Min(25,iCurrent.Length())),TPoint(3,122));
        gc.DrawText(_L("Only small index on C:"),TPoint(3,146));gc.DrawText(_L("5 START   7 SEND"),TPoint(3,168));
        gc.DrawText(_L("0 CANCEL   # EXIT"),TPoint(3,193));gc.DiscardFont();
    }
};
class CDumpUi:public CAknAppUi {
    CDumpView* iView;
public:CDumpUi():iView(0){}void ConstructL(){BaseConstructL();iView=new(ELeave)CDumpView;iView->ConstructL();AddToStackL(iView);iView->SetFocus(ETrue);}
    ~CDumpUi(){if(iView){RemoveFromStack(iView);delete iView;}}
    void HandleCommandL(TInt c){if(c==0x7401){iView->Start();return;}if(c==EEikCmdExit||c==EAknSoftkeyExit){iView->Cancel();Exit();}}
};
class CDumpDocument:public CAknDocument {public:CDumpDocument(CEikApplication& a):CAknDocument(a){}private:CEikAppUi* CreateAppUiL(){return new(ELeave)CDumpUi;}};
class CDumpApplication:public CAknApplication {private:TUid AppDllUid()const{return KDumpUid;}CApaDocument* CreateDocumentL(){return new(ELeave)CDumpDocument(*this);}};
EXPORT_C CApaApplication* NewApplication(){return new CDumpApplication;}
GLDEF_C TInt E32Dll(TDllReason){return KErrNone;}
