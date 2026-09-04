// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Network/LAN/WuwaLanSessionTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WuwaLanSessionSubsystem.generated.h"

class UNetDriver;
class UWorld;
struct FWuwaLanSessionSubsystemState;

DECLARE_LOG_CATEGORY_EXTERN(LogWuwaLanSession, Log, All);
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaLanSessionOperationChanged, EWuwaLanSessionOperation);
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaLanRoomsChanged, const TArray<FWuwaLanRoomEntry>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaLanSessionError, const FText&);

/** 管理局域网房间发现与 Listen Server 旅行的跨地图子系统 */
UCLASS()
class WUWA_API UWuwaLanSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual ~UWuwaLanSessionSubsystem() override;

	//~ Begin UGameInstanceSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UGameInstanceSubsystem Interface

	/**
     * 创建并广播局域网房间
     *
     * @param RoomName			房间显示名
     * @param PublicConnections	包含房主在内的最大玩家数
     * @return 创建请求是否成功提交
     */
	bool CreateRoom(const FString& RoomName, int32 PublicConnections);

	/** @return 搜索请求是否成功提交 */
	bool FindRooms();

	/**
     * 加入当前搜索批次中的房间
     *
     * @param SearchResultIndex	原始搜索结果索引
     * @return 加入请求是否成功提交
     */
	bool JoinRoom(int32 SearchResultIndex);

	/** @return 当前唯一局域网房间操作 */
	EWuwaLanSessionOperation GetOperation() const;

	/** @return 当前搜索批次的可加入房间 */
	const TArray<FWuwaLanRoomEntry>& GetRooms() const;

	/** @return 本地 OnlineSubsystem 是否持有命名 Session */
	bool HasActiveSession() const;

	/** 操作状态改变事件 */
	FWuwaLanSessionOperationChanged OnOperationChanged;

	/** 房间列表整体替换事件 */
	FWuwaLanRoomsChanged OnRoomsChanged;

	/** 面向界面与日志的统一错误事件 */
	FWuwaLanSessionError OnError;

private:
	/** Online 接口、搜索结果与委托句柄的非反射状态 */
	TSharedPtr<FWuwaLanSessionSubsystemState> State;

	/** 当前唯一操作 */
	EWuwaLanSessionOperation Operation = EWuwaLanSessionOperation::Idle;

	/** 当前搜索批次的房间快照 */
	TArray<FWuwaLanRoomEntry> Rooms;

	/**
     * 更新当前操作并通知界面
     *
     * @param NewOperation	新的唯一操作
     */
	void SetOperation(EWuwaLanSessionOperation NewOperation);

	/**
     * 拒绝请求并广播可见错误
     *
     * @param Error	完整失败原因
     * @return 固定返回 false
     */
	bool RejectOperation(const FText& Error);

	/**
     * 清理已创建或已加入的 Session 后恢复操作
     *
     * @param Error	清理完成后广播的原始失败原因
     */
	void BeginSessionCleanup(const FText& Error);

	/** 创建 Session 完成回调 */
	void HandleCreateSessionComplete(FName CompletedSessionName, bool bWasSuccessful);

	/** 启动 Session 完成回调 */
	void HandleStartSessionComplete(FName CompletedSessionName, bool bWasSuccessful);

	/** 搜索 Session 完成回调 */
	void HandleFindSessionsComplete(bool bWasSuccessful);

	/**
     * 加入 Session 完成回调
     *
     * @param CompletedSessionName	完成的 Session 名
     * @param bWasSuccessful		加入是否成功
     * @param ResultName			OnlineSubsystem 结果名
     */
	void HandleJoinSessionComplete(FName CompletedSessionName, bool bWasSuccessful, const FString& ResultName);

	/** 清理 Session 完成回调 */
	void HandleDestroySessionComplete(FName CompletedSessionName, bool bWasSuccessful);

	/** 成功创建并启动房间后进入当前地图的 Listen Server */
	void TravelToHostedMap();

	/** 成功地图旅行后结束 Travelling 状态 */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** 处理地图旅行失败并清理失效 Session */
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	/** 处理网络失败并清理失效 Session */
	void HandleNetworkFailure(UWorld* World,
	                          UNetDriver* NetDriver,
	                          ENetworkFailure::Type FailureType,
	                          const FString& ErrorString);
};
