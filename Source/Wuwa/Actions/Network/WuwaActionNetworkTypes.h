#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionTypes.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"
#include "WuwaActionNetworkTypes.generated.h"

/** 服务端复制给 Simulated Proxy 的 Legacy Action 表现状态 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaReplicatedLegacyActionPresentation
{
	GENERATED_BODY()

	/** 跨端稳定动作序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** 本地 Registry 解析 Definition 使用的 ActionTag */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FGameplayTag ActionTag;

	/** 服务端同步世界时间中的开始时刻 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float ServerStartTime = 0.f;

	/** 服务端是否仍持有对应 Action */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	bool bActive = false;

	/** 非活动状态携带的最终结束原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaActionEndReason EndReason = EWuwaActionEndReason::None;

	/** @return 活动表现字段是否完整且有限 */
	bool IsActivePayloadValid() const
	{
		return bActive && NetworkGeneration.IsValid() && ActionTag.IsValid() && FMath::IsFinite(ServerStartTime) &&
		       ServerStartTime >= 0.f && EndReason == EWuwaActionEndReason::None;
	}

	/** 清空跨 Avatar 表现状态 */
	void Reset()
	{
		*this = FWuwaReplicatedLegacyActionPresentation();
	}
};

/** Legacy Action 网络运行时只读证据 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionNetworkRuntimeSnapshot
{
	GENERATED_BODY()

	/** 组件是否已完成唯一依赖绑定 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	bool bInitialized = false;

	/** ActionTag 到 Definition 的唯一 Registry 条目数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 RegistryCount = 0;

	/** 当前本机 Coordinator Action 的 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration ActiveLocalGeneration;

	/** 当前本机 Coordinator Action 的本地 Handle */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaActionHandle ActiveLocalHandle;

	/** 最近服务端实际裁决的 Legacy Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration LastAuthorityGeneration;

	/** 当前只读远端表现 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration PresentationGeneration;

	/** 最近一次移动响应 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionResponse LastResponse;

	/** 本机 Coordinator 成功启动次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 LocalStartCount = 0;

	/** 服务端处理器调用次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 AuthorityProcessorCount = 0;

	/** Owning Client 接受响应次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 AcceptedResponseCount = 0;

	/** Owning Client 拒绝响应次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 RejectedResponseCount = 0;

	/** 当前等待服务端响应的取消命令数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 PendingActionExitCommandCount = 0;

	/** Owning Client 接受取消响应次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 AcceptedActionExitResponseCount = 0;

	/** Owning Client 拒绝取消响应次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 RejectedActionExitResponseCount = 0;

	/** 最近完成清理的服务端动作启动序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration LastFinalizedAuthorityActionGeneration;

	/** Avatar 运行态集中失效的累计次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 AvatarInvalidationCount = 0;

	/** 最近一次 Avatar 失效使用的 Action 结束原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaActionEndReason LastAvatarInvalidationActionEndReason = EWuwaActionEndReason::None;

	/** 最近一次 Avatar 失效使用的 Grapple 结束原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaGrappleMovementEndReason LastAvatarInvalidationGrappleEndReason = EWuwaGrappleMovementEndReason::None;

	/** Simulated Proxy 实际开始远端 Montage 的次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 PresentationStartCount = 0;

	/** 当前是否持有只读远端表现 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	bool bPresentationActive = false;

	/** Grapple 预测、权威 Query 与校正证据 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaGrappleNetworkRuntimeSnapshot Grapple;
};
