#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/WuwaStateTagTypes.h"
#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "WuwaActionAbilityInteropComponent.generated.h"

class UWuwaAbilitySystemComponent;
class UWuwaAbilityInputRouterComponent;
class UWuwaActionNetworkComponent;
class UWuwaStateTagComponent;

/** GAS Combat 状态与 Legacy 状态镜像的只读运行快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionAbilityInteropRuntimeSnapshot
{
	GENERATED_BODY()

	/** 当前是否持有完整且匹配的 ASC 与 StateTag 绑定 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	bool bInitialized = false;

	/** ASC 当前攻击状态标签计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 AttackingTagCount = 0;

	/** ASC 当前死亡状态标签计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 DeadTagCount = 0;

	/** ASC 当前硬直状态标签计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 StaggeredTagCount = 0;

	/** ASC 当前移动输入阻止标签计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 MoveBlockTagCount = 0;

	/** 是否持有攻击状态的唯一 Legacy 镜像 Handle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	bool bAttackingMirrored = false;

	/** 是否持有死亡状态的唯一 Legacy 镜像 Handle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	bool bDeadMirrored = false;

	/** 是否持有硬直状态的唯一 Legacy 镜像 Handle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	bool bStaggeredMirrored = false;

	/** 是否持有移动输入阻止状态的唯一 Legacy 镜像 Handle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	bool bMoveBlockMirrored = false;

	/** 硬直进入触发集中动作失效的累计次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 StaggerInvalidationCount = 0;

	/** 成功建立新绑定的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 InitializationGeneration = 0;

	/** 实际释放已有绑定的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interop")
	int32 ShutdownGeneration = 0;
};

/** 将 ASC Combat Tag Count 唯一镜像到 Character Legacy StateTag 的组件 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaActionAbilityInteropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建不参与 Tick 和复制的跨域状态镜像组件 */
	UWuwaActionAbilityInteropComponent();

	/**
	 * 建立当前 Avatar 的 ASC 与 Legacy StateTag 镜像
	 *
	 * @param InAbilitySystemComponent	当前 Avatar 的 Wuwa ASC
	 * @param InStateTagComponent		当前 Character 的 Legacy 状态组件
	 * @param InActionNetworkComponent	当前 Character 的 Action 网络组件
	 * @param InAbilityInputRouterComponent	当前 Character 的 Ability 输入路由
	 * @return 是否建立了完整且可幂等复用的绑定
	 */
	bool Initialize(UWuwaAbilitySystemComponent* InAbilitySystemComponent,
	                UWuwaStateTagComponent* InStateTagComponent,
	                UWuwaActionNetworkComponent* InActionNetworkComponent,
	                UWuwaAbilityInputRouterComponent* InAbilityInputRouterComponent);

	/**
	 * 解绑 ASC 事件并释放全部 Combat Legacy 镜像
	 *
	 * @return 无
	 */
	void Shutdown();

	/** @return 当前是否持有完整且匹配当前 Owner 的绑定 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem|Interop")
	bool IsInitialized() const;

	/** @return 当前 Combat Tag Count 与镜像资源的只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem|Interop")
	FWuwaActionAbilityInteropRuntimeSnapshot GetRuntimeSnapshot() const;

	bool HasBlockingActiveAbility(EWuwaAbilityInterruptSource Source) const;

	/**
	 * 查询当前 Blocking Ability 是否允许被指定输入打断。
	 *
	 * @param Source
	 *        Move / Jump / Action 等输入来源。
	 *
	 * @param IncomingActionTag
	 *        只有 Source == Action 时通常需要传入，
	 *        例如 Dash、Backstep 对应的 ActionTag。
	 */
	bool CanInterruptActiveAbility(EWuwaAbilityInterruptSource Source,
	                               const FGameplayTag& IncomingActionTag = FGameplayTag()) const;

	/**
	 * 真正请求打断当前 Blocking Ability。
	 *
	 * 内部最终转发给 UWuwaAbilitySystemComponent。
	 *
	 * @return
	 * true  = 找到 Blocking Ability，并成功执行打断；
	 * false = 没有 Blocking Ability，或者当前不能打断。
	 */
	bool TryInterruptActiveAbility(EWuwaAbilityInterruptSource Source,
	                               const FGameplayTag& IncomingActionTag = FGameplayTag());

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前绑定的 Wuwa ASC */
	TWeakObjectPtr<UWuwaAbilitySystemComponent> BoundAbilitySystemComponent;

	/** 当前绑定的 Legacy 状态组件 */
	TWeakObjectPtr<UWuwaStateTagComponent> BoundStateTagComponent;

	/** 当前绑定的 Action 网络组件 */
	TWeakObjectPtr<UWuwaActionNetworkComponent> BoundActionNetworkComponent;

	/** 当前绑定的 Ability 输入路由 */
	TWeakObjectPtr<UWuwaAbilityInputRouterComponent> BoundAbilityInputRouterComponent;

	/** ASC 攻击状态标签事件绑定 */
	FDelegateHandle AttackingTagDelegateHandle;

	/** ASC 死亡状态标签事件绑定 */
	FDelegateHandle DeadTagDelegateHandle;

	/** ASC 硬直状态标签事件绑定 */
	FDelegateHandle StaggeredTagDelegateHandle;

	/** ASC 移动输入阻止标签事件绑定 */
	FDelegateHandle MoveBlockTagDelegateHandle;

	/** 攻击状态的唯一 Legacy 镜像 Handle */
	FWuwaStateTagHandle AttackingMirrorHandle;

	/** 死亡状态的唯一 Legacy 镜像 Handle */
	FWuwaStateTagHandle DeadMirrorHandle;

	/** 硬直状态的唯一 Legacy 镜像 Handle */
	FWuwaStateTagHandle StaggeredMirrorHandle;

	/** 移动输入阻止状态的唯一 Legacy 镜像 Handle */
	FWuwaStateTagHandle MoveBlockMirrorHandle;

	/** 成功建立新绑定的次数 */
	int32 InitializationGeneration = 0;

	/** 实际释放已有绑定的次数 */
	int32 ShutdownGeneration = 0;

	/** 硬直进入触发集中动作失效的累计次数 */
	int32 StaggerInvalidationCount = 0;

	/**
	 * 响应 ASC 攻击标签计数变化
	 *
	 * @param Tag		发生变化的攻击标签
	 * @param NewCount	变化后的 ASC 标签计数
	 */
	void HandleAttackingTagChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * 响应 ASC 死亡标签计数变化
	 *
	 * @param Tag		发生变化的死亡标签
	 * @param NewCount	变化后的 ASC 标签计数
	 */
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * 响应 ASC 硬直标签计数变化
	 *
	 * @param Tag		发生变化的硬直标签
	 * @param NewCount	变化后的 ASC 标签计数
	 * @return 无
	 */
	void HandleStaggeredTagChanged(const FGameplayTag Tag, int32 NewCount);

	/**
	 * 响应 ASC 移动输入阻止标签计数变化
	 *
	 * @param Tag		发生变化的移动输入阻止标签
	 * @param NewCount	变化后的 ASC 标签计数
	 * @return 无
	 */
	void HandleMoveBlockTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 执行硬直首次进入时的输入与动作世代清理 */
	void HandleStaggeredEntered();

	/**
	 * 将一个 ASC Tag Count 收敛为零个或一个 Legacy Handle
	 *
	 * @param Tag		需要镜像的精确状态标签
	 * @param NewCount	当前 ASC 标签计数
	 * @param MirrorHandle	该标签的唯一 Legacy 镜像 Handle
	 */
	void SynchronizeMirror(const FGameplayTag& Tag, int32 NewCount, FWuwaStateTagHandle& MirrorHandle);
};
