#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "Camera/Feedback/WuwaCameraFeedbackTypes.h"
#include "Components/ActorComponent.h"
#include "Traversal/Data/WuwaGrappleActionDefinition.h"
#include "WuwaGrapplePresentationComponent.generated.h"

class AWuwaCharacter;
class UNiagaraComponent;
class USplineMeshComponent;
class UWuwaActionCoordinatorComponent;
class UWuwaActionNetworkComponent;
class UWuwaCameraModeComponent;
class UWuwaCharacterMessageDispatcherComponent;
class UWuwaCharacterMovementComponent;
class UWuwaGrappleCapabilityComponent;

/** 一次 Grapple 表现资源的 Handle 隔离记录 */
USTRUCT()
struct FWuwaGrapplePresentationRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FWuwaActionHandle ActionHandle;

	/** 当前表现记录所属跨端动作序号 */
	UPROPERTY()
	FWuwaNetworkActionGeneration NetworkGeneration;

	UPROPERTY()
	FWuwaGrappleMovementHandle MovementHandle;

	UPROPERTY()
	FVector VisualAnchorLocation = FVector::ZeroVector;

	UPROPERTY()
	FWuwaGrapplePresentationSpec PresentationSpec;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> RopeComponent = nullptr;

	UPROPERTY()
	TObjectPtr<USplineMeshComponent> RopeSplineComponent = nullptr;

	UPROPERTY()
	FWuwaCameraFeedbackRequestHandle CameraFeedbackHandle;

	float RopeFadeElapsed = 0.f;
	bool bRopeFading = false;
	bool bReleaseStarted = false;
	bool bFinalized = false;

	/** 当前记录是否由服务端只读表现状态创建 */
	bool bRemotePresentation = false;
};

/** Grapple 绳索与短时镜头反馈的可选只读消费者 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaGrapplePresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaGrapplePresentationComponent();

	/**
     * 注入同一角色的只读 Gameplay 来源与表现出口
     * @param InCharacter	当前组件所属角色
     * @param InMovementComponent	Grapple Movement 快照来源
     * @param InGrappleCapability	冻结锚点与配置来源
     * @param InDispatcher	Action Event 观察来源
     * @param InCoordinator	Started 与 Finalized 观察来源
     * @param InActionNetwork	远端 Definition Registry 来源
     * @param InCameraMode	可选镜头反馈出口
     * @return 是否完成表现事件装配
     */
	bool Initialize(AWuwaCharacter* InCharacter,
	                UWuwaCharacterMovementComponent* InMovementComponent,
	                UWuwaGrappleCapabilityComponent* InGrappleCapability,
	                UWuwaCharacterMessageDispatcherComponent* InDispatcher,
	                UWuwaActionCoordinatorComponent* InCoordinator,
	                UWuwaActionNetworkComponent* InActionNetwork,
	                UWuwaCameraModeComponent* InCameraMode);

	/** @return 是否已经绑定全部必需的只读 Gameplay 来源 */
	bool IsInitialized() const;

	/** @return 当前仍持有的 Handle 隔离表现记录数 */
	int32 GetPresentationRecordCount() const
	{
		return Records.Num();
	}

	/** @return 当前服务端复制的 Grapple 只读表现状态 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Grapple|Presentation")
	FWuwaReplicatedGrapplePresentationState GetReplicatedPresentationState() const
	{
		return GrapplePresentationState;
	}

	/** 清理换 Pawn、死亡或取消拥有关系留下的全部表现资源 */
	void InvalidateAvatarPresentation();

	/** 应用服务端复制的 Grapple 只读表现状态 */
	UFUNCTION()
	void OnRep_GrapplePresentationState();

protected:
	//~ Begin UActorComponent Interface
	virtual void
	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	void HandleActionStarted(const FWuwaActionResult& Result);
	void HandleActionEventPublished(const FWuwaActionEventMessage& Message);
	void HandleActionFinalized(const FWuwaActionFinalizedMessage& Message);
	void HandleGrappleMovementFact(const FWuwaGrappleMovementFact& Fact);

	/** @return 与 Handle 精确匹配的表现记录 */
	FWuwaGrapplePresentationRecord* FindRecord(const FWuwaActionHandle& Handle);

	/** @return 与跨端 Generation 精确匹配的表现记录 */
	FWuwaGrapplePresentationRecord* FindRecord(const FWuwaNetworkActionGeneration& Generation);

	/** @param Record 创建绳索的目标记录 */
	bool CreateRope(FWuwaGrapplePresentationRecord& Record);

	/** @param Record 开始绳索淡出的目标记录 */
	void BeginRopeFade(FWuwaGrapplePresentationRecord& Record);

	/** @param Record 立即释放目标记录持有的绳索 */
	void DestroyRope(FWuwaGrapplePresentationRecord& Record);

	/** 解绑事件并强制释放全部来源资源 */
	void ResetRuntimeState();

	UPROPERTY(Transient)
	TObjectPtr<AWuwaCharacter> CharacterOwner;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaCharacterMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaGrappleCapabilityComponent> GrappleCapability;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaCharacterMessageDispatcherComponent> Dispatcher;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaActionCoordinatorComponent> Coordinator;

	/** 远端表现解析 Definition 使用的本地 Registry */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaActionNetworkComponent> ActionNetwork;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaCameraModeComponent> CameraMode;

	UPROPERTY(Transient)
	TArray<FWuwaGrapplePresentationRecord> Records;

	/** 服务端复制给非 Owner 的 Grapple 绳索与阶段事实 */
	UPROPERTY(ReplicatedUsing = OnRep_GrapplePresentationState)
	FWuwaReplicatedGrapplePresentationState GrapplePresentationState;

	bool bInitialized = false;
};
