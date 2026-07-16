#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Actions/WuwaActionTypes.h"
#include "Core/WuwaStateTagTypes.h"

#include "WuwaActionRouterComponent.generated.h"

class UWuwaActionDefinition;
class UWuwaInputBufferComponent;
class UWuwaStateTagComponent;

struct FWuwaInputCommand;

// 保存当前动作的唯一运行态
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActiveActionRuntime
{
    GENERATED_BODY()

    // 强引用保证动作期间Definition不会被GC回收
    UPROPERTY(Transient)
    TObjectPtr<UWuwaActionDefinition> Definition = nullptr;

    // 保存启动动作的执行者，结束时必须通知同一个对象
    UPROPERTY(Transient)
    TObjectPtr<UObject> ExecutorObject = nullptr;

    UPROPERTY(Transient)
    FGameplayTag ActionTag;

    // 动作开始后不再重新读取输入上下文
    UPROPERTY(Transient)
    FWuwaActionContext Context;

    UPROPERTY(Transient)
    uint32 SourceInputSequence = 0;

    // 结束时只是放当前动作取得的标签句柄
    UPROPERTY(Transient)
    TArray<FWuwaStateTagHandle> GrantedTagHandles;

    bool IsActive() const
    {
        return Definition != nullptr && ExecutorObject != nullptr && ActionTag.IsValid();
    }

    // 只能在外部完成资源释放后调用
    void ClearAfterCleanup();
};

// 管理动作准入、仲裁、生命周期
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaActionRouterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWuwaActionRouterComponent();

    // Character 显式注入同级组件，Router 不自行搜索依赖
    bool Initialize(UWuwaInputBufferComponent *InInputBufferComponent, UWuwaStateTagComponent *InStateTagComponent);

    // 设置负责把 Input Command 转换为 Request 的对象
    bool SetActionSource(UObject *InActionSourceObject);

    // 尝试从 FIFO 队首构建并执行一个动作请求
    void TryConsumeBuffer();

    // 判断请求是否满足基础准入条件
    bool CanStart(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const;

    // 提交动作请求并返回结构化结果
    FWuwaActionResult Request(const FWuwaActionRequest &Request);

    // 主动取消当前动作
    bool CancelCurrent();

    // 由 Executor 或 Gameplay 系统结束当前动作
    bool FinishCurrent(EWuwaActionEndReason EndReason);

    UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
    bool IsInitialized() const;

    UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
    bool HasActiveAction() const
    {
        return ActiveAction.IsActive();
    }

    UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
    FGameplayTag GetCurrentActionTag() const
    {
        return ActiveAction.ActionTag;
    }

    UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
    FWuwaActionResult GetLastResult() const
    {
        return LastResult;
    }

    UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
    EWuwaActionEndReason GetLastEndReason() const
    {
        return LastEndReason;
    }

public:
    // 注册一个由 Character 装配的动作执行者
    bool RegisterExecutor(UObject *ExecutorObject);

    // 活动 Executor 必须先结束动作才能注销
    bool UnregisterExecutor(UObject *ExecutorObject);

    UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
    int32 GetRegisteredExecutorCount() const
    {
        return RegisteredExecutors.Num();
    }

protected:
    // Owner销毁时清理当前动作
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // Router 只使用组件公开接口，不拥有输入队列
    UPROPERTY(Transient)
    TObjectPtr<UWuwaInputBufferComponent> InputBufferComponent;

    // 所有活动标签仍由 StateTagComponent 统一拥有
    UPROPERTY(Transient)
    TObjectPtr<UWuwaStateTagComponent> StateTagComponent;

    // Router 唯一保存当前独占动作
    UPROPERTY(Transient)
    FWuwaActiveActionRuntime ActiveAction;

    // 保存最近一次动作结束原因供 Debug 使用
    UPROPERTY(Transient)
    EWuwaActionEndReason LastEndReason = EWuwaActionEndReason::None;

    // 检查请求快照是否包含一致且有限的数据
    bool IsContextValid(const FWuwaActionRequest &Request) const;

    // 判断失败请求是否仍可保留在 FIFO 中
    bool CanRemainBuffered(const FWuwaInputCommand &Command, const FWuwaActionRequest &Request, const EWuwaActionRejectionReason RejectionReason, double CurrentTime) const;

    // 所有 Router 时间判断使用同一个 World 时间基准
    double GetActionTime() const;

    // Source 只构建请求，不参与动作准入
    UPROPERTY(Transient)
    TObjectPtr<UObject> ActionSourceObject;

    // 防止结束回调递归消费同一条队首命令
    bool bIsConsumingBuffer = false;

    // 判断新动作是否有资格打断当前动作
    bool CanInterruptCurrent(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const;

    // 返回第一个明确支持该 Definition 的 Executor
    UObject *ResolveExecutor(const UWuwaActionDefinition &Definition) const;

    // Router 强引用已注册 Executor，避免动作期间失效
    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> RegisteredExecutors;

    // 取得标签并调用 Executor 启动动作
    bool StartApprovedAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason);

    // 保存最近一次请求结果供 Debug 使用
    UPROPERTY(Transient)
    FWuwaActionResult LastResult;

    // 防止 Executor 在启动回调中递归提交请求
    bool bIsProcessingRequest = false;

    // 防止结束回调重复进入清理流程。
    bool bIsFinishingCurrent = false;

    // 结束当前动作，但暂时不消费下一条输入
    bool FinishCurrentInternal(EWuwaActionEndReason EndReason);

    // 释放指定动作取得的全部标签句柄
    void ReleaseGrantedTagHandles(TArray<FWuwaStateTagHandle> &Handles);
};
