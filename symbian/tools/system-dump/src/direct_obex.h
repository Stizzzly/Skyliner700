#ifndef SYSTEM_DUMP_DIRECT_OBEX_H
#define SYSTEM_DUMP_DIRECT_OBEX_H
#include <e32base.h>
#include <bt_sock.h>
#include <btextnotifiers.h>
#include <btsdp.h>
#include <obex.h>
class MTransfer {
public:
    virtual TBool NextObjectL(TFileName& source,TFileName& name)=0;
    virtual void ObjectSentL(const TDesC& name)=0;
    virtual void TransferState(TInt phase,TInt error)=0;
};
// Direct OBEX Object Push: no messaging MTM, attachment copies, or archive.
class CDirectObex:public CActive,public MSdpAgentNotifier,public MSdpAttributeValueVisitor {
public:
    enum {Selecting=1,Discovering,Connecting,Putting,Disconnecting,Done,Failed,ServiceReady,OpeningFile};
    CDirectObex(MTransfer& observer):CActive(EPriorityStandard),iObserver(observer),iAgent(0),iPattern(0),
        iClient(0),iObject(0),iPhase(0),iChannel(0),iWantChannel(EFalse),iNotifierOpen(EFalse){CActiveScheduler::Add(this);}
    ~CDirectObex(){Stop();}
    void StartL(){
        Stop();User::LeaveIfError(iNotifier.Connect());iNotifierOpen=ETrue;
        iFilter().SetUUID(TUUID(0x1105));iPhase=Selecting;iObserver.TransferState(iPhase,KErrNone);
        iNotifier.StartNotifierAndGetResponse(iStatus,KDeviceSelectionNotifierUid,iFilter,iDevice);SetActive();
    }
    void Stop(){
        Cancel();if(iAgent)iAgent->Cancel();delete iAgent;iAgent=0;delete iPattern;iPattern=0;
        delete iClient;iClient=0;delete iObject;iObject=0;
        if(iNotifierOpen){iNotifier.CancelNotifier(KDeviceSelectionNotifierUid);iNotifier.Close();iNotifierOpen=EFalse;}
    }
private:
    void DoCancel(){
        if(iPhase==Selecting)iNotifier.CancelNotifier(KDeviceSelectionNotifierUid);
        else if(iClient){
            // The client owns the user-space completion pointer. Destroy it
            // before completing any still-pending outer request ourselves.
            iClient->Abort();delete iClient;iClient=0;
            if(iStatus==KRequestPending){TRequestStatus* status=&iStatus;User::RequestComplete(status,KErrCancel);}
        }
    }
    void Fail(TInt error){TInt failedAt=iPhase;iPhase=Failed;iObserver.TransferState(failedAt,error);}
    TInt RunError(TInt error){Fail(error);return KErrNone;}
    void RunL(){
        TInt error=iStatus.Int();
        if(iPhase==Disconnecting&&(error==KErrNone||error==KErrDisconnected)){iPhase=Done;iObserver.TransferState(iPhase,KErrNone);return;}
        if(error!=KErrNone){Fail(error);return;}
        if(iPhase==Selecting){
            if(!iDevice().IsValidBDAddr())User::Leave(KErrNotFound);
            iPhase=Discovering;iObserver.TransferState(iPhase,KErrNone);
            iAgent=CSdpAgent::NewL(*this,iDevice().BDAddr());iPattern=CSdpSearchPattern::NewL();
            iPattern->AddL(TUUID(0x1105));iAgent->SetRecordFilterL(*iPattern);iAgent->NextRecordRequestL();
        }else if(iPhase==ServiceReady){
            delete iAgent;iAgent=0;delete iPattern;iPattern=0;
            TObexBluetoothProtocolInfo protocol;protocol.iTransport.Copy(_L("RFCOMM"));
            protocol.iAddr.SetBTAddr(iDevice().BDAddr());protocol.iAddr.SetPort(iChannel);
            iClient=CObexClient::NewL(protocol);iPhase=Connecting;iObserver.TransferState(iPhase,KErrNone);
            iClient->Connect(iStatus);SetActive();
        }else if(iPhase==Connecting)PutNextL();
        else if(iPhase==Putting){iObserver.ObjectSentL(iName);PutNextL();}
    }
    void PutNextL(){
        delete iObject;iObject=0;
        if(!iObserver.NextObjectL(iSource,iName)){
            iPhase=Disconnecting;iObserver.TransferState(iPhase,KErrNone);iClient->Disconnect(iStatus);SetActive();return;
        }
        iPhase=OpeningFile;iObserver.TransferState(iPhase,KErrNone);
        iObject=CObexFileObject::NewL();iObject->InitFromFileL(iSource);
        iObject->SetNameL(iName);iObject->SetTypeL(_L8("application/octet-stream"));
        iPhase=Putting;iObserver.TransferState(iPhase,KErrNone);iClient->Put(*iObject,iStatus);SetActive();
    }
    void NextRecordRequestComplete(TInt error,TSdpServRecordHandle handle,TInt count){
        if(error!=KErrNone||!count){Fail(error==KErrNone||error==KErrEof?KErrNotFound:error);return;}
        iChannel=0;iWantChannel=EFalse;
        TRAPD(result,iAgent->AttributeRequestL(handle,KSdpAttrIdProtocolDescriptorList));if(result!=KErrNone)Fail(result);
    }
    void AttributeRequestResult(TSdpServRecordHandle,TSdpAttributeID,CSdpAttrValue* value){
        TRAPD(error,value->AcceptVisitorL(*this));delete value;if(error!=KErrNone)Fail(error);
    }
    void AttributeRequestComplete(TSdpServRecordHandle,TInt error){
        if(iPhase==Failed)return;
        if(error==KErrNone&&iChannel>=1&&iChannel<=30){
            iPhase=ServiceReady;iStatus=KRequestPending;SetActive();TRequestStatus* s=&iStatus;User::RequestComplete(s,KErrNone);
        }else {TRAPD(result,iAgent->NextRecordRequestL());if(result!=KErrNone)Fail(result);}
    }
    void VisitAttributeValueL(CSdpAttrValue& value,TSdpElementType type){
        if(type==ETypeUUID)iWantChannel=(value.UUID()==TUUID(KRFCOMM));
        else if(type==ETypeUint&&iWantChannel){iChannel=value.Uint();iWantChannel=EFalse;}
    }
    void StartListL(CSdpAttrValueList&){}
    void EndListL(){iWantChannel=EFalse;}
    MTransfer& iObserver;RNotifier iNotifier;TBTDeviceSelectionParamsPckg iFilter;TBTDeviceResponseParamsPckg iDevice;
    CSdpAgent* iAgent;CSdpSearchPattern* iPattern;CObexClient* iClient;CObexFileObject* iObject;
    TInt iPhase,iChannel;TBool iWantChannel,iNotifierOpen;TFileName iSource,iName;
};
#endif
