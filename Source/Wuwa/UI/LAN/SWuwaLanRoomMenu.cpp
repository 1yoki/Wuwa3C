// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LAN/SWuwaLanRoomMenu.h"

#include "Network/LAN/WuwaLanSessionSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "WuwaLanRoomMenu"

SWuwaLanRoomMenu::~SWuwaLanRoomMenu()
{
	if (UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get())
	{
		Subsystem->OnOperationChanged.Remove(OperationChangedHandle);
		Subsystem->OnRoomsChanged.Remove(RoomsChangedHandle);
		Subsystem->OnError.Remove(ErrorHandle);
	}
}

void SWuwaLanRoomMenu::Construct(const FArguments& InArgs)
{
	SessionSubsystem = InArgs._SessionSubsystem;

	ChildSlot[SNew(SBorder)
	              .Padding(32.f)
	              .HAlign(HAlign_Center)
	              .VAlign(VAlign_Center)
	              .BorderBackgroundColor(
	                  FLinearColor(0.01f, 0.015f, 0.025f, 0.94f))[SNew(SBox).WidthOverride(760.f).HeightOverride(
	                  620.f)[SNew(SVerticalBox)

	                         + SVerticalBox::Slot().AutoHeight().Padding(
	                               0.f, 0.f, 0.f, 20.f)[SNew(STextBlock)
	                                                        .Text(LOCTEXT("Title", "局域网房间"))
	                                                        .Justification(ETextJustify::Center)
	                                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
	                                                        .ColorAndOpacity(FLinearColor::White)]

	                         + SVerticalBox::Slot().AutoHeight().Padding(
	                               0.f, 0.f, 0.f, 8.f)[SNew(STextBlock)
	                                                       .Text(LOCTEXT("RoomNameLabel", "房间名"))
	                                                       .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))]

	                         + SVerticalBox::Slot().AutoHeight().Padding(
	                               0.f, 0.f, 0.f, 12.f)[SAssignNew(RoomNameInput, SEditableTextBox)
	                                                        .Text(LOCTEXT("DefaultRoomName", "Wuwa 局域网房间"))
	                                                        .HintText(LOCTEXT("RoomNameHint", "输入 1～24 个字符"))
	                                                        .SelectAllTextWhenFocused(true)]

	                         + SVerticalBox::Slot().AutoHeight().Padding(
	                               0.f, 0.f, 0.f, 8.f)[SNew(STextBlock)
	                                                       .Text(LOCTEXT("PlayerCountLabel", "最大玩家数（包含房主）"))
	                                                       .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))]

	                         + SVerticalBox::Slot().AutoHeight().Padding(
	                               0.f, 0.f, 0.f, 16.f)[SAssignNew(PlayerCountInput, SSpinBox<int32>)
	                                                        .MinValue(WuwaLanSession::MinimumPublicConnections)
	                                                        .MaxValue(WuwaLanSession::MaximumPublicConnections)
	                                                        .MinSliderValue(WuwaLanSession::MinimumPublicConnections)
	                                                        .MaxSliderValue(WuwaLanSession::MaximumPublicConnections)
	                                                        .Value(WuwaLanSession::DefaultPublicConnections)]

	                         + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
	                               [SNew(SUniformGridPanel).SlotPadding(FMargin(6.f, 0.f))

	                                + SUniformGridPanel::Slot(
	                                      0, 0)[SNew(SButton)
	                                                .Text(LOCTEXT("CreateRoom", "创建房间"))
	                                                .HAlign(HAlign_Center)
	                                                .IsEnabled(this, &SWuwaLanRoomMenu::CanCreateRoom)
	                                                .OnClicked(this, &SWuwaLanRoomMenu::HandleCreateRoomClicked)]

	                                + SUniformGridPanel::Slot(
	                                      1, 0)[SNew(SButton)
	                                                .Text(LOCTEXT("RefreshRooms", "刷新房间"))
	                                                .HAlign(HAlign_Center)
	                                                .IsEnabled(this, &SWuwaLanRoomMenu::CanFindRooms)
	                                                .OnClicked(this, &SWuwaLanRoomMenu::HandleFindRoomsClicked)]]

	                         + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)[SNew(SSeparator)]

	                         + SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 0.f, 0.f, 12.f)
	                               [SAssignNew(RoomListView, SListView<TSharedPtr<FWuwaLanRoomEntry>>)
	                                    .ListItemsSource(&RoomItems)
	                                    .SelectionMode(ESelectionMode::Single)
	                                    .OnGenerateRow(this, &SWuwaLanRoomMenu::GenerateRoomRow)
	                                    .OnSelectionChanged(this, &SWuwaLanRoomMenu::HandleRoomSelectionChanged)]

	                         + SVerticalBox::Slot().AutoHeight().Padding(
	                               0.f, 0.f, 0.f, 12.f)[SNew(SButton)
	                                                        .Text(LOCTEXT("JoinSelectedRoom", "加入所选房间"))
	                                                        .HAlign(HAlign_Center)
	                                                        .IsEnabled(this, &SWuwaLanRoomMenu::CanJoinRoom)
	                                                        .OnClicked(this, &SWuwaLanRoomMenu::HandleJoinRoomClicked)]

	                         + SVerticalBox::Slot()
	                               .AutoHeight()[SAssignNew(StatusText, STextBlock)
	                                                 .Text(LOCTEXT("InitialStatus",
	                                                               "创建一个房间，或刷新列表加入同一局域网内的房间"))
	                                                 .Justification(ETextJustify::Center)
	                                                 .AutoWrapText(true)
	                                                 .ColorAndOpacity(FLinearColor(0.7f, 0.82f, 1.f))]]]];

	if (UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get())
	{
		OperationChangedHandle =
		    Subsystem->OnOperationChanged.AddSP(SharedThis(this), &SWuwaLanRoomMenu::HandleOperationChanged);
		RoomsChangedHandle = Subsystem->OnRoomsChanged.AddSP(SharedThis(this), &SWuwaLanRoomMenu::HandleRoomsChanged);
		ErrorHandle = Subsystem->OnError.AddSP(SharedThis(this), &SWuwaLanRoomMenu::HandleError);
		HandleRoomsChanged(Subsystem->GetRooms());
		HandleOperationChanged(Subsystem->GetOperation());
	}
	else
	{
		HandleError(LOCTEXT("MissingSubsystem", "房间系统不可用：未取得局域网 Session 子系统"));
	}
}

bool SWuwaLanRoomMenu::CanCreateRoom() const
{
	const UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get();
	return IsValid(Subsystem) && Subsystem->GetOperation() == EWuwaLanSessionOperation::Idle &&
	       !Subsystem->HasActiveSession();
}

bool SWuwaLanRoomMenu::CanFindRooms() const
{
	const UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get();
	return IsValid(Subsystem) && Subsystem->GetOperation() == EWuwaLanSessionOperation::Idle &&
	       !Subsystem->HasActiveSession();
}

bool SWuwaLanRoomMenu::CanJoinRoom() const
{
	const UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get();
	return IsValid(Subsystem) && Subsystem->GetOperation() == EWuwaLanSessionOperation::Idle &&
	       !Subsystem->HasActiveSession() && SelectedRoom.IsValid() && SelectedRoom->IsJoinable();
}

FReply SWuwaLanRoomMenu::HandleCreateRoomClicked()
{
	UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get();
	if (!IsValid(Subsystem) || !RoomNameInput.IsValid() || !PlayerCountInput.IsValid())
	{
		HandleError(LOCTEXT("CreateInvalidUI", "无法创建房间：界面状态无效"));
		return FReply::Handled();
	}

	Subsystem->CreateRoom(RoomNameInput->GetText().ToString(), PlayerCountInput->GetValue());
	return FReply::Handled();
}

FReply SWuwaLanRoomMenu::HandleFindRoomsClicked()
{
	UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get();
	if (!IsValid(Subsystem))
	{
		HandleError(LOCTEXT("FindInvalidUI", "无法刷新房间：房间系统不可用"));
		return FReply::Handled();
	}

	SelectedRoom.Reset();
	Subsystem->FindRooms();
	return FReply::Handled();
}

FReply SWuwaLanRoomMenu::HandleJoinRoomClicked()
{
	UWuwaLanSessionSubsystem* Subsystem = SessionSubsystem.Get();
	if (!IsValid(Subsystem) || !SelectedRoom.IsValid())
	{
		HandleError(LOCTEXT("JoinInvalidUI", "无法加入房间：请先选择一个有效房间"));
		return FReply::Handled();
	}

	Subsystem->JoinRoom(SelectedRoom->SearchResultIndex);
	return FReply::Handled();
}

TSharedRef<ITableRow> SWuwaLanRoomMenu::GenerateRoomRow(TSharedPtr<FWuwaLanRoomEntry> Room,
                                                        const TSharedRef<STableViewBase>& OwnerTable)
{
	const FString RowText = Room.IsValid() ? FString::Printf(TEXT("%s    房主：%s    玩家：%d/%d    Ping：%d ms"),
	                                                         *Room->RoomName,
	                                                         *Room->OwnerName,
	                                                         Room->CurrentPlayers,
	                                                         Room->MaximumPlayers,
	                                                         Room->PingInMilliseconds)
	                                       : TEXT("无效房间");

	return SNew(STableRow<TSharedPtr<FWuwaLanRoomEntry>>, OwnerTable)
	    .Padding(FMargin(10.f, 8.f))
	        [SNew(STextBlock).Text(FText::FromString(RowText)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))];
}

void SWuwaLanRoomMenu::HandleRoomSelectionChanged(TSharedPtr<FWuwaLanRoomEntry> Room,
                                                  const ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	SelectedRoom = MoveTemp(Room);
}

void SWuwaLanRoomMenu::HandleOperationChanged(const EWuwaLanSessionOperation NewOperation)
{
	if (!StatusText.IsValid())
	{
		return;
	}

	StatusText->SetColorAndOpacity(FLinearColor(0.7f, 0.82f, 1.f));

	switch (NewOperation)
	{
		case EWuwaLanSessionOperation::Creating:
			StatusText->SetText(LOCTEXT("Creating", "正在创建局域网房间……"));
			break;
		case EWuwaLanSessionOperation::Starting:
			StatusText->SetText(LOCTEXT("Starting", "房间已创建，正在启动 Listen Server……"));
			break;
		case EWuwaLanSessionOperation::Searching:
			StatusText->SetText(LOCTEXT("Searching", "正在搜索同一局域网内的房间……"));
			break;
		case EWuwaLanSessionOperation::Joining:
			StatusText->SetText(LOCTEXT("Joining", "正在加入所选房间……"));
			break;
		case EWuwaLanSessionOperation::Travelling:
			StatusText->SetText(LOCTEXT("Travelling", "连接成功，正在进入游戏地图……"));
			break;
		case EWuwaLanSessionOperation::Recovering:
			StatusText->SetText(LOCTEXT("Recovering", "正在清理失败的房间状态……"));
			break;
		case EWuwaLanSessionOperation::Idle:
		default:
			break;
	}
}

void SWuwaLanRoomMenu::HandleRoomsChanged(const TArray<FWuwaLanRoomEntry>& Rooms)
{
	SelectedRoom.Reset();
	RoomItems.Reset(Rooms.Num());
	for (const FWuwaLanRoomEntry& Room : Rooms)
	{
		RoomItems.Add(MakeShared<FWuwaLanRoomEntry>(Room));
	}

	if (RoomListView.IsValid())
	{
		RoomListView->ClearSelection();
		RoomListView->RequestListRefresh();
	}

	if (StatusText.IsValid())
	{
		StatusText->SetColorAndOpacity(FLinearColor(0.7f, 0.82f, 1.f));
		StatusText->SetText(RoomItems.IsEmpty()
		                        ? LOCTEXT("NoRooms", "未发现可加入房间，请确认两台电脑位于同一局域网后刷新")
		                        : FText::Format(LOCTEXT("RoomsFound", "发现 {0} 个可加入房间"), RoomItems.Num()));
	}
}

void SWuwaLanRoomMenu::HandleError(const FText& Error)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(Error);
		StatusText->SetColorAndOpacity(FLinearColor(1.f, 0.32f, 0.25f));
	}
}

#undef LOCTEXT_NAMESPACE
