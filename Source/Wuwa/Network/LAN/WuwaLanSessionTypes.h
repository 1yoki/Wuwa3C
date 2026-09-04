// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 局域网房间当前正在执行的唯一操作 */
enum class EWuwaLanSessionOperation : uint8
{
	Idle,
	Creating,
	Starting,
	Searching,
	Joining,
	Travelling,
	Recovering,
};

namespace WuwaLanSession
{
inline const FName SessionName(NAME_GameSession);
inline const FName RoomNameKey(TEXT("WUWA_ROOM_NAME"));
inline const FName ProductKey(TEXT("WUWA_PRODUCT"));
inline constexpr TCHAR ProductIdentifier[] = TEXT("Wuwa");
inline constexpr int32 MinimumRoomNameLength = 1;
inline constexpr int32 MaximumRoomNameLength = 24;
inline constexpr int32 MinimumPublicConnections = 2;
inline constexpr int32 MaximumPublicConnections = 8;
inline constexpr int32 DefaultPublicConnections = 4;
inline constexpr int32 MaximumSearchResults = 100;

/** @return 去除首尾空白后的房间名 */
inline FString NormalizeRoomName(const FString& RoomName)
{
	FString Normalized = RoomName;
	Normalized.TrimStartAndEndInline();
	return Normalized;
}

/**
 * 验证标准化后的房间名
 *
 * @param RoomName	待验证的标准化房间名
 * @return 房间名是否满足长度与字符约束
 */
inline bool IsValidRoomName(const FString& RoomName)
{
	if (RoomName.Len() < MinimumRoomNameLength || RoomName.Len() > MaximumRoomNameLength)
	{
		return false;
	}

	for (const TCHAR Character : RoomName)
	{
		if (FChar::IsControl(Character))
		{
			return false;
		}
	}
	return true;
}

/**
 * 验证公开连接数
 *
 * @param PublicConnections	待验证的最大玩家数
 * @return 最大玩家数是否位于局域网房间允许范围
 */
inline bool IsValidPublicConnections(const int32 PublicConnections)
{
	return PublicConnections >= MinimumPublicConnections && PublicConnections <= MaximumPublicConnections;
}
}

/** 供原生房间菜单展示的稳定房间快照 */
struct FWuwaLanRoomEntry
{
	/** 原始搜索结果中的索引 */
	int32 SearchResultIndex = INDEX_NONE;

	/** 房主设置的房间名 */
	FString RoomName;

	/** OnlineSubsystem 返回的房主显示名 */
	FString OwnerName;

	/** 当前已占用的公开连接数 */
	int32 CurrentPlayers = 0;

	/** 最大公开连接数 */
	int32 MaximumPlayers = 0;

	/** 当前可加入的公开连接数 */
	int32 OpenPublicConnections = 0;

	/** 搜索结果测得的延迟 */
	int32 PingInMilliseconds = 0;

	/** @return 当前快照是否可用于加入房间 */
	bool IsJoinable() const
	{
		return SearchResultIndex >= 0 && WuwaLanSession::IsValidRoomName(RoomName) && !OwnerName.IsEmpty() &&
		       WuwaLanSession::IsValidPublicConnections(MaximumPlayers) && CurrentPlayers >= 0 &&
		       CurrentPlayers < MaximumPlayers && OpenPublicConnections > 0 && OpenPublicConnections <= MaximumPlayers;
	}
};
