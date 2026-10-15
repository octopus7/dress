#include "Session/LaceLoftWardrobeSubsystem.h"
#include "Garments/LaceLoftPatternDefinition.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "Fitting/LaceLoftJointLimits.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr uint8 SaveMagic[8]={'L','L','S','A','V','E','0','1'};
constexpr int32 SaveEnvelopeSize=28;
constexpr int32 MaximumProjects=128,MaximumSaveBytes=16*1024*1024;
bool ValidPose(FName Pose)
{return Pose==TEXT("Neutral") || Pose==TEXT("ArmsRaised") || Pose==TEXT("ArmsForward") || Pose==TEXT("Sitting");}
bool VersionMatches(const FLaceLoftGarmentProject& Project,const ULaceLoftPatternDefinition& Pattern)
{
    return Project.PatternVersion==Pattern.PatternVersion && Project.SourceHash==Pattern.SourceHash
        && Project.DummyMeasurementsVersion==Pattern.DummyMeasurementsVersion;
}
void Digest(const uint8* Data,int32 Size,uint8* Result)
{FMD5 Hash;Hash.Update(Data,Size);Hash.Final(Result);}
}

void ULaceLoftWardrobeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{Super::Initialize(Collection);LoadSession();}
void ULaceLoftWardrobeSubsystem::Deinitialize()
{EndStroke();Models.Reset();Patterns.Reset();State=nullptr;Super::Deinitialize();}
void ULaceLoftWardrobeSubsystem::EnsureState()
{if(!State)State=NewObject<ULaceLoftSaveGame>(this);}

ULaceLoftPatternDefinition* ULaceLoftWardrobeSubsystem::GetPattern(FName Id)
{
    if(auto* Found=Patterns.Find(Id))return Found->Get();
    if(Id!=TEXT("TShirt") && Id!=TEXT("DolphinShorts"))return nullptr;
    const FString Name=TEXT("DA_")+Id.ToString();
    auto* Pattern=LoadObject<ULaceLoftPatternDefinition>(nullptr,*(TEXT("/Game/Garments/Patterns/")+Name+TEXT(".")+Name));
    if(Pattern)Patterns.Add(Id,Pattern);
    return Pattern;
}
ULaceLoftJointLimits* ULaceLoftWardrobeSubsystem::GetJointLimits()
{
    if(!JointLimits)JointLimits=LoadObject<ULaceLoftJointLimits>(nullptr,TEXT("/Game/Characters/Dummy/Animation/DA_DummyJointLimits.DA_DummyJointLimits"));
    return JointLimits;
}
const TArray<FLaceLoftGarmentProject>& ULaceLoftWardrobeSubsystem::GetProjects() const
{static const TArray<FLaceLoftGarmentProject> Empty;return State?State->Projects:Empty;}
const FLaceLoftGarmentProject* ULaceLoftWardrobeSubsystem::GetProject(FGuid Id) const
{return State?State->Projects.FindByPredicate([Id](const auto& P){return P.ProjectId==Id;}):nullptr;}
FLaceLoftGarmentProject* ULaceLoftWardrobeSubsystem::MutableProject(FGuid Id)
{return State?State->Projects.FindByPredicate([Id](const auto& P){return P.ProjectId==Id;}):nullptr;}
const FLaceLoftGarmentProject* ULaceLoftWardrobeSubsystem::GetCurrentProject() const
{return State?GetProject(State->ActiveProjectId):nullptr;}
bool ULaceLoftWardrobeSubsystem::RebuildModel(FLaceLoftGarmentProject& Project)
{
    auto* Pattern=GetPattern(Project.PatternId);
    if(!Pattern || !VersionMatches(Project,*Pattern))return false;
    FLaceLoftSewingModel Model;
    if(!Model.Configure(*Pattern,Project.Progress))return false;
    for(auto& Entry:Project.Progress)Entry.Value=Model.Progress(Entry.Key);
    for(const auto& Entry:Project.PiecePositions)
        if(!Pattern->FindPiece(Entry.Key) || Entry.Value.ContainsNaN() || Entry.Value.GetAbsMax()>10000)return false;
    Models.Add(Project.ProjectId,MoveTemp(Model));return true;
}
FLaceLoftSewingModel* ULaceLoftWardrobeSubsystem::GetModel(FGuid Id)
{
    if(auto* Model=Models.Find(Id))return Model;
    auto* Project=MutableProject(Id);
    return Project && RebuildModel(*Project)?Models.Find(Id):nullptr;
}
FGuid ULaceLoftWardrobeSubsystem::CreateProject(FName Id)
{
    EndStroke();EnsureState();auto* Pattern=GetPattern(Id);
    if(!Pattern || !ValidatePattern(*Pattern).IsEmpty())return {};
    if(State->Projects.Num()>=MaximumProjects)
    {StatusMessage=FText::FromString(TEXT("작업은 최대 128개까지 보관할 수 있습니다. 기존 작업을 선택해 주세요."));OnChanged.Broadcast();return {};}
    FLaceLoftGarmentProject Project;Project.ProjectId=FGuid::NewGuid();Project.PatternId=Id;
    Project.PatternVersion=Pattern->PatternVersion;Project.SourceHash=Pattern->SourceHash;
    Project.DummyMeasurementsVersion=Pattern->DummyMeasurementsVersion;
    if(!RebuildModel(Project))return {};
    State->Projects.Add(Project);State->ActiveProjectId=Project.ProjectId;
    SaveSession();OnChanged.Broadcast();return Project.ProjectId;
}
bool ULaceLoftWardrobeSubsystem::SelectProject(FGuid Id)
{
    EndStroke();if(!GetProject(Id))return false;
    State->ActiveProjectId=Id;SaveSession();OnChanged.Broadcast();return true;
}
bool ULaceLoftWardrobeSubsystem::BeginCut(FName Id,FVector2D Point)
{
    EndStroke();const auto* Project=GetCurrentProject();if(!Project)return false;
    auto* Model=GetModel(Project->ProjectId);
    if(!Model || !Model->BeginCut(Id,Point))return false;
    StrokeProjectId=Project->ProjectId;return true;
}
bool ULaceLoftWardrobeSubsystem::BeginSeam(FName Id,int32 Index,FVector2D Point)
{
    EndStroke();const auto* Project=GetCurrentProject();if(!Project)return false;
    auto* Model=GetModel(Project->ProjectId);
    if(!Model || !Model->BeginSeam(Id,Index,Point))return false;
    StrokeProjectId=Project->ProjectId;return true;
}
bool ULaceLoftWardrobeSubsystem::UpdateStroke(FVector2D Point)
{auto* Model=Models.Find(StrokeProjectId);return Model && Model->UpdateStroke(Point);}
bool ULaceLoftWardrobeSubsystem::EndStroke()
{
    const FGuid Id=StrokeProjectId;StrokeProjectId.Invalidate();
    auto* Model=Models.Find(Id);auto* Project=MutableProject(Id);
    if(!Model || !Project)return false;
    const auto Change=Model->EndStroke();if(!Change)return false;
    FLaceLoftEditCommand Command;Command.Before=Project->Progress;Command.PositionsBefore=Project->PiecePositions;
    Project->Progress.Add(Change->PathId,Change->After);RebuildModel(*Project);
    Command.After=Project->Progress;Command.PositionsAfter=Project->PiecePositions;
    Commit(*Project,Command);return true;
}
void ULaceLoftWardrobeSubsystem::Commit(FLaceLoftGarmentProject& Project,const FLaceLoftEditCommand& Command)
{
    Project.UndoCommands.Add(Command);Project.RedoCommands.Reset();
    RefreshEquipment();SaveSession();OnChanged.Broadcast();
}
bool ULaceLoftWardrobeSubsystem::Undo()
{
    EndStroke();auto* Project=State?MutableProject(State->ActiveProjectId):nullptr;
    if(!Project || Project->UndoCommands.IsEmpty())return false;
    const auto Command=Project->UndoCommands.Pop();Project->Progress=Command.Before;Project->PiecePositions=Command.PositionsBefore;
    Project->RedoCommands.Add(Command);RebuildModel(*Project);RefreshEquipment();SaveSession();OnChanged.Broadcast();return true;
}
bool ULaceLoftWardrobeSubsystem::Redo()
{
    EndStroke();auto* Project=State?MutableProject(State->ActiveProjectId):nullptr;
    if(!Project || Project->RedoCommands.IsEmpty())return false;
    const auto Command=Project->RedoCommands.Pop();Project->Progress=Command.After;Project->PiecePositions=Command.PositionsAfter;
    Project->UndoCommands.Add(Command);RebuildModel(*Project);RefreshEquipment();SaveSession();OnChanged.Broadcast();return true;
}
bool ULaceLoftWardrobeSubsystem::MovePiece(FName Id,FVector2D Point)
{
    EndStroke();auto* Project=State?MutableProject(State->ActiveProjectId):nullptr;
    auto* Model=Project?GetModel(Project->ProjectId):nullptr;
    if(!Model || !Model->IsPieceCut(Id) || Point.ContainsNaN() || Point.GetAbsMax()>10000)return false;
    if(Project->PiecePositions.FindRef(Id).Equals(Point,.001))return false;
    FLaceLoftEditCommand Command;Command.Before=Project->Progress;Command.PositionsBefore=Project->PiecePositions;
    Project->PiecePositions.Add(Id,Point);Command.After=Project->Progress;Command.PositionsAfter=Project->PiecePositions;
    Commit(*Project,Command);return true;
}
bool ULaceLoftWardrobeSubsystem::CompleteCurrentProjectCheat()
{
    EndStroke();auto* Project=State?MutableProject(State->ActiveProjectId):nullptr;
    auto* Model=Project?GetModel(Project->ProjectId):nullptr;
    auto* Pattern=Project?GetPattern(Project->PatternId):nullptr;
    if(!Model || !Pattern || !VersionMatches(*Project,*Pattern) || Model->IsCompleted())return false;
    FLaceLoftEditCommand Command;Command.Before=Project->Progress;Command.PositionsBefore=Project->PiecePositions;
    const auto CompletePath=[&](FName Id,double Length)
    {FLaceLoftPathProgress Progress;Progress.Intervals.Emplace(0.,Length);Command.After.Add(Id,MoveTemp(Progress));};
    for(const auto& Piece:Pattern->Pieces)CompletePath(Piece.CutOutline.PathId,Piece.CutOutline.LengthMm());
    for(const auto& Seam:Pattern->Seams)for(int32 I=0;I<Seam.SideA.Num();++I)
    {
        const auto& Ref=Seam.SideA[I];
        CompletePath(FLaceLoftSewingModel::SeamPathId(Seam.SeamId,I),Pattern->FindPiece(Ref.PieceId)->FindEdge(Ref.EdgeId)->Path.LengthMm());
    }
    FLaceLoftSewingModel Completed;
    if(!Completed.Configure(*Pattern,Command.After) || !Completed.IsCompleted())return false;
    Command.PositionsAfter=Project->PiecePositions;Project->Progress=Command.After;
    Models.Add(Project->ProjectId,MoveTemp(Completed));Commit(*Project,Command);return true;
}
bool ULaceLoftWardrobeSubsystem::CanEquip(FGuid Id)
{
    const auto* Project=GetProject(Id);auto* Pattern=Project?GetPattern(Project->PatternId):nullptr;
    if(!Pattern || !VersionMatches(*Project,*Pattern) || !Pattern->ClothAsset.LoadSynchronous())return false;
    auto* Model=GetModel(Id);return Model && Model->IsCompleted();
}
bool ULaceLoftWardrobeSubsystem::Equip(FGuid Id)
{
    if(!CanEquip(Id))return false;
    const auto Slot=GetPattern(GetProject(Id)->PatternId)->Slot;
    (Slot==ELaceLoftGarmentSlot::Top?State->EquippedTop:State->EquippedBottom)=Id;
    SaveSession();OnChanged.Broadcast();return true;
}
void ULaceLoftWardrobeSubsystem::Unequip(ELaceLoftGarmentSlot Slot)
{EnsureState();(Slot==ELaceLoftGarmentSlot::Top?State->EquippedTop:State->EquippedBottom).Invalidate();SaveSession();OnChanged.Broadcast();}
FGuid ULaceLoftWardrobeSubsystem::GetEquipped(ELaceLoftGarmentSlot Slot) const
{return State?(Slot==ELaceLoftGarmentSlot::Top?State->EquippedTop:State->EquippedBottom):FGuid();}
void ULaceLoftWardrobeSubsystem::RefreshEquipment()
{
    if(!State)return;
    for(const auto Slot:{ELaceLoftGarmentSlot::Top,ELaceLoftGarmentSlot::Bottom})
    {
        auto& Id=Slot==ELaceLoftGarmentSlot::Top?State->EquippedTop:State->EquippedBottom;
        if(!CanEquip(Id) || GetPattern(GetProject(Id)->PatternId)->Slot!=Slot)Id.Invalidate();
    }
}

bool ULaceLoftWardrobeSubsystem::SaveSession()
{
    EnsureState();
    if(bStorageBlocked || StrokeProjectId.IsValid())
    {StatusMessage=FText::FromString(TEXT("입력을 마친 뒤 저장하세요. 복구 원본을 보존하지 못한 경우 저장이 잠깁니다."));return false;}
    RefreshEquipment();TArray<uint8> Payload;
    if(!UGameplayStatics::SaveGameToMemory(State,Payload))
    {StatusMessage=FText::FromString(TEXT("작업을 저장 데이터로 변환하지 못했습니다. 현재 작업은 열려 있습니다."));return false;}
    if(Payload.Num()>MaximumSaveBytes-SaveEnvelopeSize || State->Projects.Num()>MaximumProjects)
    {StatusMessage=FText::FromString(TEXT("저장 용량 한도(16MB)를 넘었습니다. 이전 저장 파일은 보존되며 현재 변경은 아직 저장되지 않았습니다."));return false;}
    TArray<uint8> Bytes;Bytes.SetNumUninitialized(SaveEnvelopeSize+Payload.Num());
    FMemory::Memcpy(Bytes.GetData(),SaveMagic,8);const int32 Size=Payload.Num();FMemory::Memcpy(Bytes.GetData()+8,&Size,4);
    Digest(Payload.GetData(),Size,Bytes.GetData()+12);FMemory::Memcpy(Bytes.GetData()+SaveEnvelopeSize,Payload.GetData(),Size);
    // Windows generic slots otherwise truncate the only good file before writing.
    // Stage and verify on the same volume, flush, then atomically replace the primary.
    const FString Pending=SaveSlot+TEXT("_pending_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<uint8> Readback;bool Saved=false;
    if(UGameplayStatics::SaveDataToSlot(Bytes,Pending,0) && UGameplayStatics::LoadDataFromSlot(Readback,Pending,0) && Readback==Bytes)
    {
        const FString From=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Pending+TEXT(".sav")));
        const FString To=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("SaveGames")/(SaveSlot+TEXT(".sav")));
        TUniquePtr<IFileHandle> Handle(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*From,true));
        const bool Flushed=Handle && Handle->Flush(true);Handle.Reset();
        bool Promote=Flushed;
#if WITH_DEV_AUTOMATION_TESTS
        Promote &= !bFailSavePromotion;
#endif
#if PLATFORM_WINDOWS
        if(Promote)Saved=::MoveFileExW(*From,*To,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        // This release targets Win64. Never risk truncating a valid slot on an unsupported platform.
        Saved=false;
#endif
    }
    if(!Saved)UGameplayStatics::DeleteGameInSlot(Pending,0);
    StatusMessage=FText::FromString(Saved?TEXT("저장했습니다."):TEXT("저장하지 못했습니다. 현재 작업은 열려 있습니다. 저장을 다시 시도하세요."));
    return Saved;
}
bool ULaceLoftWardrobeSubsystem::PreserveRecovery(const TArray<uint8>& Bytes)
{
    RecoverySlot=SaveSlot+TEXT("_recovery_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if(!UGameplayStatics::SaveDataToSlot(Bytes,RecoverySlot,0))
    {bStorageBlocked=true;StatusMessage=FText::FromString(TEXT("이전 파일을 보존하지 못해 저장을 잠갔습니다. 파일 권한을 확인하세요."));return false;}
    StatusMessage=FText::FromString(TEXT("저장 파일의 손상 또는 버전 차이를 발견했습니다. 원본을 보관하고 복구 가능한 작업만 열었습니다. 보관: ")+RecoverySlot);
    return true;
}
bool ULaceLoftWardrobeSubsystem::LoadSession()
{
    StrokeProjectId.Invalidate();Models.Reset();State=NewObject<ULaceLoftSaveGame>(this);bStorageBlocked=false;RecoverySlot.Reset();StatusMessage=FText::GetEmpty();
    TArray<uint8> Bytes;
    if(!UGameplayStatics::DoesSaveGameExist(SaveSlot,0))return true;
    if(!UGameplayStatics::LoadDataFromSlot(Bytes,SaveSlot,0))
    {
        if(UGameplayStatics::DoesSaveGameExist(SaveSlot,0))
        {bStorageBlocked=true;StatusMessage=FText::FromString(TEXT("저장 파일을 읽지 못했습니다. 원본을 덮어쓰지 않습니다."));return false;}
        return true;
    }
    bool Valid=Bytes.Num()>=SaveEnvelopeSize && Bytes.Num()<=MaximumSaveBytes;
    int32 PayloadSize=0;uint8 Hash[16];
    if(Valid)
    {
        FMemory::Memcpy(&PayloadSize,Bytes.GetData()+8,4);
        Valid=FMemory::Memcmp(Bytes.GetData(),SaveMagic,8)==0 && PayloadSize==Bytes.Num()-SaveEnvelopeSize;
    }
    if(Valid){Digest(Bytes.GetData()+SaveEnvelopeSize,PayloadSize,Hash);Valid=FMemory::Memcmp(Hash,Bytes.GetData()+12,16)==0;}
    ULaceLoftSaveGame* Loaded=nullptr;
    if(Valid)
    {
        TArray<uint8> Payload;Payload.Append(Bytes.GetData()+SaveEnvelopeSize,PayloadSize);
        Loaded=Cast<ULaceLoftSaveGame>(UGameplayStatics::LoadGameFromMemory(Payload));
        Valid=Loaded && Loaded->SchemaVersion==1 && Loaded->Projects.Num()<=MaximumProjects;
    }
    if(!Valid){PreserveRecovery(Bytes);OnChanged.Broadcast();return false;}
    bool Recovered=false;TSet<FGuid> Seen;
    for(auto Project:Loaded->Projects)
    {
        bool ProjectValid=Project.ProjectId.IsValid() && !Seen.Contains(Project.ProjectId) && RebuildModel(Project);
        if(ProjectValid)
        {
            auto* Pattern=GetPattern(Project.PatternId);
            for(const auto* Commands:{&Project.UndoCommands,&Project.RedoCommands})for(const auto& Command:*Commands)
            {
                FLaceLoftSewingModel Check;
                if(!Check.Configure(*Pattern,Command.Before) || !Check.Configure(*Pattern,Command.After)){ProjectValid=false;break;}
                for(const auto* Positions:{&Command.PositionsBefore,&Command.PositionsAfter})for(const auto& Pair:*Positions)
                    if(!Pattern->FindPiece(Pair.Key) || Pair.Value.ContainsNaN() || Pair.Value.GetAbsMax()>10000)ProjectValid=false;
            }
        }
        if(ProjectValid){Seen.Add(Project.ProjectId);State->Projects.Add(MoveTemp(Project));}
        else{Models.Remove(Project.ProjectId);Recovered=true;}
    }
    State->ActiveProjectId=Seen.Contains(Loaded->ActiveProjectId)?Loaded->ActiveProjectId:
        (State->Projects.IsEmpty()?FGuid():State->Projects[0].ProjectId);
    State->EquippedTop=Loaded->EquippedTop;State->EquippedBottom=Loaded->EquippedBottom;
    if(ValidPose(Loaded->PoseId))State->PoseId=Loaded->PoseId;else Recovered=true;
    for(const auto& Joint:Loaded->JointRotations)
    {
        const auto R=Joint.Value;
        const auto* Limits=GetJointLimits();const auto* Range=Limits?Limits->Joints.Find(Joint.Key):nullptr;
        if(!Range || !Range->IsValid() || R.ContainsNaN() || Range->Clamp(R)!=R)
        {Recovered=true;continue;}
        State->JointRotations.Add(Joint);
    }
    RefreshEquipment();if(Recovered)PreserveRecovery(Bytes);OnChanged.Broadcast();return !Recovered;
}

FName ULaceLoftWardrobeSubsystem::GetPose() const{return State?State->PoseId:FName(TEXT("Neutral"));}
void ULaceLoftWardrobeSubsystem::SetPose(FName Pose)
{
    if(!ValidPose(Pose))return;
    EnsureState();State->PoseId=Pose;State->JointRotations.Reset();SaveSession();OnChanged.Broadcast();
}
const TMap<FName,FRotator>& ULaceLoftWardrobeSubsystem::GetJointRotations() const
{static const TMap<FName,FRotator> Empty;return State?State->JointRotations:Empty;}
void ULaceLoftWardrobeSubsystem::SetJointRotation(FName Joint,FRotator Rotation,bool bSave)
{
    const auto* Limits=GetJointLimits();const auto* Range=Limits?Limits->Joints.Find(Joint):nullptr;
    if(Rotation.ContainsNaN() || !Range || !Range->IsValid())return;EnsureState();
    State->JointRotations.Add(Joint,Range->Clamp(Rotation));
    if(bSave)SaveSession();OnChanged.Broadcast();
}
void ULaceLoftWardrobeSubsystem::ResetJoints(){EnsureState();State->JointRotations.Reset();SaveSession();OnChanged.Broadcast();}

