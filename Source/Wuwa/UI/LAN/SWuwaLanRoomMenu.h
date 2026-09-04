// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/LAN/WuwaLanSessionTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SEditableTextBox;
class STextBlock;
class UWuwaLanSessionSubsystem;
template <typename NumericType> class SSpinBox;

/** 创建、搜索并加入局域网房间的原生菜单 */
class SWuwaLanRoomMenu final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWuwaLanRoomMenu) {}

	SLATE_ARGUMENT(UWuwaLanSessionSubsystem*, SessionSubsystem)
	SLATE_END_ARGS()

	virtual ~SWuwaLanRoomMenu() override;

	/**
     * 构建局域网房间菜单
     *
     * @param InArgs	包含 SessionSubsystem 的 Slate 参数
     */
	void Construct(const FArguments& InArgs);

private:
	/** 房间子系统弱引用 */
	TWeakObjectPtr<UWuwaLanSessionSubsystem> SessionSubsystem;

	/** 房间名输入框 */
	TSharedPtr<SEditableTextBox> RoomNameInput;

	/** 最大玩家数输入框 */
	TSharedPtr<SSpinBox<int32>> PlayerCountInput;

	/** 可选择房间列表 */
	TSharedPtr<SListView<TSharedPtr<FWuwaLanRoomEntry>>> RoomListView;

	/** 当前操作或错误状态文本 */
	TSharedPtr<STextBlock> StatusText;

	/** Slate 列表拥有的房间快照 */
	TArray<TSharedPtr<FWuwaLanRoomEntry>> RoomItems;

	/** 当前选中的房间 */
	TSharedPtr<FWuwaLanRoomEntry> SelectedRoom;

	/** 操作状态事件句柄 */
	FDelegateHandle OperationChangedHandle;

	/** 房间列表事件句柄 */
	FDelegateHandle RoomsChangedHandle;

	/** 错误事件句柄 */
	FDelegateHandle ErrorHandle;

	/** @return 创建房间按钮当前是否可用 */
	bool CanCreateRoom() const;

	/** @return 刷新房间按钮当前是否可用 */
	bool CanFindRooms() const;

	/** @return 加入所选房间按钮当前是否可用 */
	bool CanJoinRoom() const;

	/** @return 创建房间按钮响应 */
	FReply HandleCreateRoomClicked();

	/** @return 刷新房间按钮响应 */
	FReply HandleFindRoomsClicked();

	/** @return 加入房间按钮响应 */
	FReply HandleJoinRoomClicked();

	/**
     * 创建单个房间列表行
     *
     * @param Room		行对应的房间快照
     * @param OwnerTable	拥有该行的列表
     * @return 可展示的房间行
     */
	TSharedRef<ITableRow> GenerateRoomRow(TSharedPtr<FWuwaLanRoomEntry> Room,
	                                      const TSharedRef<STableViewBase>& OwnerTable);

	/** 更新当前选择的房间 */
	void HandleRoomSelectionChanged(TSharedPtr<FWuwaLanRoomEntry> Room, ESelectInfo::Type SelectInfo);

	/** 更新异步操作状态 */
	void HandleOperationChanged(EWuwaLanSessionOperation NewOperation);

	/** 整体替换房间列表 */
	void HandleRoomsChanged(const TArray<FWuwaLanRoomEntry>& Rooms);

	/** 显示可操作错误 */
	void HandleError(const FText& Error);
};
