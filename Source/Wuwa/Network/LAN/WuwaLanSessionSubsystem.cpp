// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/LAN/WuwaLanSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogWuwaLanSession);

namespace
{
/** @return 操作对应的稳定日志名称 */
const TCHAR* GetOperationName(const EWuwaLanSessionOperation Operation)
{
	switch (Operation)
	{
		case EWuwaLanSessionOperation::Idle:
			return TEXT("Idle");
		case EWuwaLanSessionOperation::Creating:
			return TEXT("Creating");
		case EWuwaLanSessionOperation::Starting:
			return TEXT("Starting");
		case EWuwaLanSessionOperation::Searching:
			return TEXT("Searching");
		case EWuwaLanSessionOperation::Joining:
			return TEXT("Joining");
		case EWuwaLanSessionOperation::Travelling:
			return TEXT("Travelling");
		case EWuwaLanSessionOperation::Recovering:
			return TEXT("Recovering");
		default:
			return TEXT("Unknown");
	}
}

/** @return 指定世界使用的 Session 接口 */
IOnlineSessionPtr ResolveSessionInterface(const UWorld* World)
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);
	return OnlineSubsystem != nullptr ? OnlineSubsystem->GetSessionInterface() : nullptr;
}
}

/** Online 接口、搜索结果与委托句柄的完整非反射状态 */
struct FWuwaLanSessionSubsystemState
{
	/** 当前世界对应的 Session 接口 */
	IOnlineSessionPtr SessionInterface;

	/** 当前或最后一次成功搜索 */
	TSharedPtr<FOnlineSessionSearch> Search;

	/** 创建回调句柄 */
	FDelegateHandle CreateHandle;

	/** 启动回调句柄 */
	FDelegateHandle StartHandle;

	/** 搜索回调句柄 */
	FDelegateHandle FindHandle;

	/** 加入回调句柄 */
	FDelegateHandle JoinHandle;

	/** 销毁回调句柄 */
	FDelegateHandle DestroyHandle;

	/** 地图加载回调句柄 */
	FDelegateHandle PostLoadMapHandle;

	/** 地图旅行失败回调句柄 */
	FDelegateHandle TravelFailureHandle;

	/** 网络失败回调句柄 */
	FDelegateHandle NetworkFailureHandle;

	/** 创建成功后重新打开的地图包名 */
	FString HostedMapPackage;

	/** Session 清理完成后需要恢复给界面的错误 */
	FText PendingCleanupError;
};

UWuwaLanSessionSubsystem::~UWuwaLanSessionSubsystem() = default;

void UWuwaLanSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	State = MakeShared<FWuwaLanSessionSubsystemState>();
	State->PostLoadMapHandle =
	    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	if (GEngine != nullptr)
	{
		State->TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
		State->NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
	}
	else
	{
		UE_LOG(LogWuwaLanSession, Error, TEXT("局域网房间初始化失败：GEngine 无效"));
	}
}

void UWuwaLanSessionSubsystem::Deinitialize()
{
	if (State.IsValid())
	{
		if (State->SessionInterface.IsValid())
		{
			if (State->CreateHandle.IsValid())
			{
				State->SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(State->CreateHandle);
			}
			if (State->StartHandle.IsValid())
			{
				State->SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(State->StartHandle);
			}
			if (State->FindHandle.IsValid())
			{
				State->SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(State->FindHandle);
			}
			if (State->JoinHandle.IsValid())
			{
				State->SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(State->JoinHandle);
			}
			if (State->DestroyHandle.IsValid())
			{
				State->SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(State->DestroyHandle);
			}
		}

		if (State->PostLoadMapHandle.IsValid())
		{
			FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(State->PostLoadMapHandle);
		}
		if (GEngine != nullptr)
		{
			if (State->TravelFailureHandle.IsValid())
			{
				GEngine->OnTravelFailure().Remove(State->TravelFailureHandle);
			}
			if (State->NetworkFailureHandle.IsValid())
			{
				GEngine->OnNetworkFailure().Remove(State->NetworkFailureHandle);
			}
		}
	}

	Rooms.Reset();
	State.Reset();
	Operation = EWuwaLanSessionOperation::Idle;

	Super::Deinitialize();
}

bool UWuwaLanSessionSubsystem::CreateRoom(const FString& RoomName, const int32 PublicConnections)
{
	if (!State.IsValid())
	{
		return RejectOperation(NSLOCTEXT("WuwaLanSession", "CreateNoState", "无法创建房间：房间子系统尚未初始化"));
	}
	if (Operation != EWuwaLanSessionOperation::Idle)
	{
		return RejectOperation(
		    FText::Format(NSLOCTEXT("WuwaLanSession", "CreateBusy", "无法创建房间：当前正在执行 {0}"),
		                  FText::FromString(GetOperationName(Operation))));
	}

	const FString NormalizedRoomName = WuwaLanSession::NormalizeRoomName(RoomName);
	if (!WuwaLanSession::IsValidRoomName(NormalizedRoomName))
	{
		return RejectOperation(
		    FText::Format(NSLOCTEXT("WuwaLanSession", "InvalidRoomName", "房间名必须为 {0}～{1} 个非控制字符"),
		                  WuwaLanSession::MinimumRoomNameLength,
		                  WuwaLanSession::MaximumRoomNameLength));
	}
	if (!WuwaLanSession::IsValidPublicConnections(PublicConnections))
	{
		return RejectOperation(
		    FText::Format(NSLOCTEXT("WuwaLanSession", "InvalidConnections", "最大玩家数必须为 {0}～{1}"),
		                  WuwaLanSession::MinimumPublicConnections,
		                  WuwaLanSession::MaximumPublicConnections));
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return RejectOperation(NSLOCTEXT("WuwaLanSession", "CreateNoWorld", "无法创建房间：当前世界无效"));
	}

	State->SessionInterface = ResolveSessionInterface(World);
	if (!State->SessionInterface.IsValid())
	{
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "CreateNoInterface", "无法创建房间：OnlineSubsystemNull 未提供 Session 接口"));
	}
	if (State->SessionInterface->GetNamedSession(WuwaLanSession::SessionName) != nullptr)
	{
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "CreateExisting", "无法创建房间：本机已持有一个房间 Session"));
	}

	State->HostedMapPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (!FPackageName::IsValidLongPackageName(State->HostedMapPackage))
	{
		return RejectOperation(
		    FText::Format(NSLOCTEXT("WuwaLanSession", "CreateInvalidMap", "无法创建房间：当前地图包名无效（{0}）"),
		                  FText::FromString(State->HostedMapPackage)));
	}

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = PublicConnections;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = true;
	Settings.bIsDedicated = false;
	Settings.bUsesPresence = false;
	Settings.bAllowJoinViaPresence = false;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bAntiCheatProtected = false;
	Settings.Set(
	    WuwaLanSession::RoomNameKey, NormalizedRoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(WuwaLanSession::ProductKey,
	             FString(WuwaLanSession::ProductIdentifier),
	             EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(SETTING_MAPNAME, State->HostedMapPackage, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	SetOperation(EWuwaLanSessionOperation::Creating);
	State->CreateHandle = State->SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
	    FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));

	const bool bSubmitted = State->SessionInterface->CreateSession(0, WuwaLanSession::SessionName, Settings);
	if (!bSubmitted && Operation == EWuwaLanSessionOperation::Creating)
	{
		State->SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(State->CreateHandle);
		State->CreateHandle.Reset();
		SetOperation(EWuwaLanSessionOperation::Idle);
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "CreateRejected", "创建房间请求未被 OnlineSubsystemNull 接受"));
	}
	return bSubmitted;
}

bool UWuwaLanSessionSubsystem::FindRooms()
{
	if (!State.IsValid())
	{
		return RejectOperation(NSLOCTEXT("WuwaLanSession", "FindNoState", "无法搜索房间：房间子系统尚未初始化"));
	}
	if (Operation != EWuwaLanSessionOperation::Idle)
	{
		return RejectOperation(FText::Format(NSLOCTEXT("WuwaLanSession", "FindBusy", "无法搜索房间：当前正在执行 {0}"),
		                                     FText::FromString(GetOperationName(Operation))));
	}

	UWorld* World = GetWorld();
	State->SessionInterface = ResolveSessionInterface(World);
	if (!State->SessionInterface.IsValid())
	{
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "FindNoInterface", "无法搜索房间：OnlineSubsystemNull 未提供 Session 接口"));
	}
	if (State->SessionInterface->GetNamedSession(WuwaLanSession::SessionName) != nullptr)
	{
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "FindExisting", "无法搜索房间：本机已位于一个房间 Session 中"));
	}

	Rooms.Reset();
	OnRoomsChanged.Broadcast(Rooms);

	State->Search = MakeShared<FOnlineSessionSearch>();
	State->Search->bIsLanQuery = true;
	State->Search->MaxSearchResults = WuwaLanSession::MaximumSearchResults;

	SetOperation(EWuwaLanSessionOperation::Searching);
	State->FindHandle = State->SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
	    FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));

	const bool bSubmitted = State->SessionInterface->FindSessions(0, State->Search.ToSharedRef());
	if (!bSubmitted && Operation == EWuwaLanSessionOperation::Searching)
	{
		State->SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(State->FindHandle);
		State->FindHandle.Reset();
		State->Search.Reset();
		SetOperation(EWuwaLanSessionOperation::Idle);
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "FindRejected", "搜索房间请求未被 OnlineSubsystemNull 接受"));
	}
	return bSubmitted;
}

bool UWuwaLanSessionSubsystem::JoinRoom(const int32 SearchResultIndex)
{
	if (!State.IsValid())
	{
		return RejectOperation(NSLOCTEXT("WuwaLanSession", "JoinNoState", "无法加入房间：房间子系统尚未初始化"));
	}
	if (Operation != EWuwaLanSessionOperation::Idle)
	{
		return RejectOperation(FText::Format(NSLOCTEXT("WuwaLanSession", "JoinBusy", "无法加入房间：当前正在执行 {0}"),
		                                     FText::FromString(GetOperationName(Operation))));
	}
	if (!State->Search.IsValid() || State->Search->SearchState != EOnlineAsyncTaskState::Done ||
	    !State->Search->SearchResults.IsValidIndex(SearchResultIndex))
	{
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "JoinInvalidResult", "无法加入房间：选择的搜索结果已失效，请重新刷新"));
	}

	const FOnlineSessionSearchResult& SearchResult = State->Search->SearchResults[SearchResultIndex];
	if (!SearchResult.IsValid() || SearchResult.Session.NumOpenPublicConnections <= 0)
	{
		return RejectOperation(NSLOCTEXT("WuwaLanSession", "JoinNotAvailable", "无法加入房间：房间无效或已经满员"));
	}

	State->SessionInterface = ResolveSessionInterface(GetWorld());
	if (!State->SessionInterface.IsValid())
	{
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "JoinNoInterface", "无法加入房间：OnlineSubsystemNull 未提供 Session 接口"));
	}
	if (State->SessionInterface->GetNamedSession(WuwaLanSession::SessionName) != nullptr)
	{
		return RejectOperation(NSLOCTEXT("WuwaLanSession", "JoinExisting", "无法加入房间：本机已持有一个房间 Session"));
	}

	SetOperation(EWuwaLanSessionOperation::Joining);
	State->JoinHandle = State->SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
	    FOnJoinSessionCompleteDelegate::CreateWeakLambda(
	        this,
	        [this](const FName CompletedSessionName, const EOnJoinSessionCompleteResult::Type Result)
	        {
		        HandleJoinSessionComplete(
		            CompletedSessionName, Result == EOnJoinSessionCompleteResult::Success, LexToString(Result));
	        }));

	const bool bSubmitted = State->SessionInterface->JoinSession(0, WuwaLanSession::SessionName, SearchResult);
	if (!bSubmitted && Operation == EWuwaLanSessionOperation::Joining)
	{
		State->SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(State->JoinHandle);
		State->JoinHandle.Reset();
		SetOperation(EWuwaLanSessionOperation::Idle);
		return RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "JoinRejected", "加入房间请求未被 OnlineSubsystemNull 接受"));
	}
	return bSubmitted;
}

EWuwaLanSessionOperation UWuwaLanSessionSubsystem::GetOperation() const
{
	return Operation;
}

const TArray<FWuwaLanRoomEntry>& UWuwaLanSessionSubsystem::GetRooms() const
{
	return Rooms;
}

bool UWuwaLanSessionSubsystem::HasActiveSession() const
{
	IOnlineSessionPtr SessionInterface = State.IsValid() ? State->SessionInterface : nullptr;
	if (!SessionInterface.IsValid())
	{
		SessionInterface = ResolveSessionInterface(GetWorld());
	}
	return SessionInterface.IsValid() && SessionInterface->GetNamedSession(WuwaLanSession::SessionName) != nullptr;
}

void UWuwaLanSessionSubsystem::SetOperation(const EWuwaLanSessionOperation NewOperation)
{
	if (Operation == NewOperation)
	{
		return;
	}

	Operation = NewOperation;
	OnOperationChanged.Broadcast(Operation);
}

bool UWuwaLanSessionSubsystem::RejectOperation(const FText& Error)
{
	UE_LOG(LogWuwaLanSession, Error, TEXT("%s"), *Error.ToString());
	OnError.Broadcast(Error);
	return false;
}

void UWuwaLanSessionSubsystem::BeginSessionCleanup(const FText& Error)
{
	if (!State.IsValid())
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(Error);
		return;
	}
	if (Operation == EWuwaLanSessionOperation::Recovering)
	{
		UE_LOG(LogWuwaLanSession, Warning, TEXT("忽略重复 Session 清理请求。Error=%s"), *Error.ToString());
		return;
	}

	State->SessionInterface = ResolveSessionInterface(GetWorld());
	if (!State->SessionInterface.IsValid() ||
	    State->SessionInterface->GetNamedSession(WuwaLanSession::SessionName) == nullptr)
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(Error);
		return;
	}

	State->PendingCleanupError = Error;
	SetOperation(EWuwaLanSessionOperation::Recovering);
	State->DestroyHandle = State->SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
	    FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));

	UE_LOG(LogWuwaLanSession, Warning, TEXT("开始清理失败操作留下的 Session。Reason=%s"), *Error.ToString());

	const bool bSubmitted = State->SessionInterface->DestroySession(WuwaLanSession::SessionName);
	if (!bSubmitted && Operation == EWuwaLanSessionOperation::Recovering)
	{
		State->SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(State->DestroyHandle);
		State->DestroyHandle.Reset();
		State->SessionInterface->RemoveNamedSession(WuwaLanSession::SessionName);

		const FText CleanupError = State->PendingCleanupError;
		State->PendingCleanupError = FText::GetEmpty();
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(CleanupError);
	}
}

void UWuwaLanSessionSubsystem::HandleCreateSessionComplete(const FName CompletedSessionName, const bool bWasSuccessful)
{
	if (!State.IsValid() || !State->SessionInterface.IsValid())
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(NSLOCTEXT("WuwaLanSession", "CreateCallbackNoState", "创建房间失败：Session 回调状态无效"));
		return;
	}

	State->SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(State->CreateHandle);
	State->CreateHandle.Reset();

	if (CompletedSessionName != WuwaLanSession::SessionName || !bWasSuccessful)
	{
		UE_LOG(LogWuwaLanSession,
		       Error,
		       TEXT("创建房间完成回调失败。Session=%s, Success=%s"),
		       *CompletedSessionName.ToString(),
		       bWasSuccessful ? TEXT("true") : TEXT("false"));
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "CreateCallbackFailed", "创建房间失败：OnlineSubsystemNull 未能创建 Session"));
		return;
	}

	SetOperation(EWuwaLanSessionOperation::Starting);
	State->StartHandle = State->SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
	    FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleStartSessionComplete));

	const bool bSubmitted = State->SessionInterface->StartSession(WuwaLanSession::SessionName);
	if (!bSubmitted && Operation == EWuwaLanSessionOperation::Starting)
	{
		State->SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(State->StartHandle);
		State->StartHandle.Reset();
		BeginSessionCleanup(NSLOCTEXT("WuwaLanSession", "StartRejected", "创建房间失败：Session 启动请求未被接受"));
	}
}

void UWuwaLanSessionSubsystem::HandleStartSessionComplete(const FName CompletedSessionName, const bool bWasSuccessful)
{
	if (!State.IsValid() || !State->SessionInterface.IsValid())
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(NSLOCTEXT("WuwaLanSession", "StartCallbackNoState", "创建房间失败：Session 启动回调状态无效"));
		return;
	}

	State->SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(State->StartHandle);
	State->StartHandle.Reset();

	if (CompletedSessionName != WuwaLanSession::SessionName || !bWasSuccessful)
	{
		UE_LOG(LogWuwaLanSession,
		       Error,
		       TEXT("启动房间完成回调失败。Session=%s, Success=%s"),
		       *CompletedSessionName.ToString(),
		       bWasSuccessful ? TEXT("true") : TEXT("false"));
		BeginSessionCleanup(
		    NSLOCTEXT("WuwaLanSession", "StartCallbackFailed", "创建房间失败：OnlineSubsystemNull 未能启动 Session"));
		return;
	}

	TravelToHostedMap();
}

void UWuwaLanSessionSubsystem::HandleFindSessionsComplete(const bool bWasSuccessful)
{
	if (!State.IsValid() || !State->SessionInterface.IsValid())
	{
		Rooms.Reset();
		OnRoomsChanged.Broadcast(Rooms);
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(NSLOCTEXT("WuwaLanSession", "FindCallbackNoState", "搜索房间失败：Session 回调状态无效"));
		return;
	}

	State->SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(State->FindHandle);
	State->FindHandle.Reset();

	if (!bWasSuccessful || !State->Search.IsValid())
	{
		State->Search.Reset();
		Rooms.Reset();
		OnRoomsChanged.Broadcast(Rooms);
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(
		    NSLOCTEXT("WuwaLanSession", "FindCallbackFailed", "搜索房间失败：OnlineSubsystemNull 未完成 LAN 搜索"));
		return;
	}

	TArray<FWuwaLanRoomEntry> FoundRooms;
	const TArray<FOnlineSessionSearchResult>& SearchResults = State->Search->SearchResults;
	FoundRooms.Reserve(SearchResults.Num());

	for (int32 Index = 0; Index < SearchResults.Num(); ++Index)
	{
		const FOnlineSessionSearchResult& Result = SearchResults[Index];
		FString Product;
		FString RoomName;
		const bool bHasProduct = Result.Session.SessionSettings.Get(WuwaLanSession::ProductKey, Product);
		const bool bHasRoomName = Result.Session.SessionSettings.Get(WuwaLanSession::RoomNameKey, RoomName);
		const int32 MaximumPlayers = Result.Session.SessionSettings.NumPublicConnections;
		const int32 OpenConnections = Result.Session.NumOpenPublicConnections;
		if (!Result.IsValid() || !bHasProduct || Product != WuwaLanSession::ProductIdentifier || !bHasRoomName ||
		    !WuwaLanSession::IsValidRoomName(RoomName) || Result.Session.OwningUserName.IsEmpty() ||
		    !WuwaLanSession::IsValidPublicConnections(MaximumPlayers) || OpenConnections <= 0 ||
		    OpenConnections > MaximumPlayers)
		{
			continue;
		}

		FWuwaLanRoomEntry& Entry = FoundRooms.Emplace_GetRef();
		Entry.SearchResultIndex = Index;
		Entry.RoomName = MoveTemp(RoomName);
		Entry.OwnerName = Result.Session.OwningUserName;
		Entry.MaximumPlayers = MaximumPlayers;
		Entry.OpenPublicConnections = OpenConnections;
		Entry.CurrentPlayers = FMath::Clamp(MaximumPlayers - OpenConnections, 0, MaximumPlayers);
		Entry.PingInMilliseconds = FMath::Max(0, Result.PingInMs);
	}

	Rooms = MoveTemp(FoundRooms);
	OnRoomsChanged.Broadcast(Rooms);
	SetOperation(EWuwaLanSessionOperation::Idle);
}

void UWuwaLanSessionSubsystem::HandleJoinSessionComplete(const FName CompletedSessionName,
                                                         const bool bWasSuccessful,
                                                         const FString& ResultName)
{
	if (!State.IsValid() || !State->SessionInterface.IsValid())
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(NSLOCTEXT("WuwaLanSession", "JoinCallbackNoState", "加入房间失败：Session 回调状态无效"));
		return;
	}

	State->SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(State->JoinHandle);
	State->JoinHandle.Reset();

	if (CompletedSessionName != WuwaLanSession::SessionName || !bWasSuccessful)
	{
		const FText Error = FText::Format(NSLOCTEXT("WuwaLanSession", "JoinCallbackFailed", "加入房间失败：{0}"),
		                                  FText::FromString(ResultName));
		UE_LOG(LogWuwaLanSession,
		       Error,
		       TEXT("加入房间完成回调失败。Session=%s, Result=%s"),
		       *CompletedSessionName.ToString(),
		       *ResultName);

		if (HasActiveSession())
		{
			BeginSessionCleanup(Error);
		}
		else
		{
			SetOperation(EWuwaLanSessionOperation::Idle);
			RejectOperation(Error);
		}
		return;
	}

	FString ConnectString;
	if (!State->SessionInterface->GetResolvedConnectString(WuwaLanSession::SessionName, ConnectString) ||
	    ConnectString.IsEmpty())
	{
		BeginSessionCleanup(NSLOCTEXT("WuwaLanSession", "JoinNoAddress", "加入房间失败：无法解析房主连接地址"));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* LocalController =
	    IsValid(GameInstance) ? GameInstance->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (!IsValid(LocalController))
	{
		BeginSessionCleanup(
		    NSLOCTEXT("WuwaLanSession", "JoinNoController", "加入房间失败：本地 PlayerController 无效"));
		return;
	}

	State->Search.Reset();
	Rooms.Reset();
	OnRoomsChanged.Broadcast(Rooms);
	SetOperation(EWuwaLanSessionOperation::Travelling);

	LocalController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void UWuwaLanSessionSubsystem::HandleDestroySessionComplete(const FName CompletedSessionName, const bool bWasSuccessful)
{
	if (!State.IsValid())
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		return;
	}

	if (State->SessionInterface.IsValid())
	{
		State->SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(State->DestroyHandle);
		if (!bWasSuccessful)
		{
			State->SessionInterface->RemoveNamedSession(WuwaLanSession::SessionName);
		}
	}
	State->DestroyHandle.Reset();

	const FText CleanupError = State->PendingCleanupError;
	State->PendingCleanupError = FText::GetEmpty();
	State->Search.Reset();
	Rooms.Reset();
	OnRoomsChanged.Broadcast(Rooms);
	SetOperation(EWuwaLanSessionOperation::Idle);

	if (!bWasSuccessful)
	{
		UE_LOG(LogWuwaLanSession,
		       Warning,
		       TEXT("Session 清理回调失败，已移除本地命名 Session。Session=%s"),
		       *CompletedSessionName.ToString());
	}
	RejectOperation(CleanupError.IsEmpty()
	                    ? NSLOCTEXT("WuwaLanSession", "CleanupUnknownFailure", "房间操作失败，Session 已清理")
	                    : CleanupError);
}

void UWuwaLanSessionSubsystem::TravelToHostedMap()
{
	if (!State.IsValid() || !FPackageName::IsValidLongPackageName(State->HostedMapPackage) || !IsValid(GetWorld()))
	{
		BeginSessionCleanup(
		    NSLOCTEXT("WuwaLanSession", "HostTravelInvalid", "创建房间失败：Listen Server 地图上下文无效"));
		return;
	}

	SetOperation(EWuwaLanSessionOperation::Travelling);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*State->HostedMapPackage), true, TEXT("listen"));
}

void UWuwaLanSessionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!IsValid(LoadedWorld) || LoadedWorld->GetGameInstance() != GetGameInstance() ||
	    Operation != EWuwaLanSessionOperation::Travelling)
	{
		return;
	}

	SetOperation(EWuwaLanSessionOperation::Idle);
}

void UWuwaLanSessionSubsystem::HandleTravelFailure(UWorld* World,
                                                   const ETravelFailure::Type FailureType,
                                                   const FString& ErrorString)
{
	if (!IsValid(World) || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	const FText Error = FText::Format(NSLOCTEXT("WuwaLanSession", "TravelFailure", "地图旅行失败：{0}（{1}）"),
	                                  FText::FromString(ETravelFailure::ToString(FailureType)),
	                                  FText::FromString(ErrorString));
	if (HasActiveSession())
	{
		BeginSessionCleanup(Error);
	}
	else
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(Error);
	}
}

void UWuwaLanSessionSubsystem::HandleNetworkFailure(UWorld* World,
                                                    UNetDriver* NetDriver,
                                                    const ENetworkFailure::Type FailureType,
                                                    const FString& ErrorString)
{
	if (!IsValid(World) || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	const FText Error = FText::Format(NSLOCTEXT("WuwaLanSession", "NetworkFailure", "网络连接失败：{0}（{1}）"),
	                                  FText::FromString(ENetworkFailure::ToString(FailureType)),
	                                  FText::FromString(ErrorString));
	UE_LOG(LogWuwaLanSession,
	       Error,
	       TEXT("捕获网络失败。Driver=%s, Type=%s, Error=%s"),
	       *GetNameSafe(NetDriver),
	       ENetworkFailure::ToString(FailureType),
	       *ErrorString);

	if (HasActiveSession())
	{
		BeginSessionCleanup(Error);
	}
	else
	{
		SetOperation(EWuwaLanSessionOperation::Idle);
		RejectOperation(Error);
	}
}
