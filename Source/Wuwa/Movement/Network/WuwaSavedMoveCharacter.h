#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"
#include "Movement/Network/WuwaMovementActionNetworkReplayState.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"

/** SavedMove 本地保存的纯标量 Grapple 重演状态 */
struct WUWA_API FWuwaGrappleSavedMoveState
{
	/** 当前状态所属跨端动作序号 */
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** Grapple 提交时的 Capsule 起点 */
	FVector CommitStartLocation = FVector::ZeroVector;

	/** 上一物理步已经应用的轨迹偏移 */
	FVector PreviousAppliedOffset = FVector::ZeroVector;

	/** 上一物理步的基础轨迹切线速度 */
	FVector LastBaseTangentVelocity = FVector::ZeroVector;

	/** 当前基础轨迹偏移 */
	FVector BaseOffset = FVector::ZeroVector;

	/** 当前 Grapple 已运行时间 */
	float ElapsedTime = 0.f;

	/** 当前 Pull 阶段归一化时间 */
	float NormalizedPullTime = 0.f;

	/** 当前冻结横向轴上的累计偏移 */
	float AccumulatedLateralOffset = 0.f;

	/** 当前冻结横向轴上的速度 */
	float LateralVelocity = 0.f;

	/** 当前 Grapple 阶段 */
	EWuwaGrappleRuntimePhase Phase = EWuwaGrappleRuntimePhase::None;

	/** 当前 Move 起点是否持有 Grapple Runtime */
	bool bActive = false;

	/** 清空全部本地重演字段 */
	void Reset()
	{
		*this = FWuwaGrappleSavedMoveState();
	}
};

/** 保存 Wuwa 自定义移动元数据的客户端预测 Move */
class WUWA_API FWuwaSavedMove_Character final : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	/** 当前 Move 捕获的唯一移动命令 */
	FWuwaPendingNetworkMovementCommand SavedNetworkCommand;

	/** 当前 Move 起点的纯标量 Grapple 物理状态 */
	FWuwaGrappleSavedMoveState SavedGrappleState;

	/** 当前 Move 起点的普通 MovementAction 最小物理状态 */
	FWuwaMovementActionNetworkReplayState SavedMovementActionState;

	//~ Begin FSavedMove_Character Interface
	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character,
	                        float InDeltaTime,
	                        const FVector& NewAcceleration,
	                        FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	//~ End FSavedMove_Character Interface
};

/** 分配 FWuwaSavedMove_Character 的客户端预测数据 */
class WUWA_API FWuwaNetworkPredictionData_Client_Character final : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	/** @param ClientMovement 使用本预测数据的移动组件 */
	explicit FWuwaNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement);

	/** @return 新的 Wuwa SavedMove */
	virtual FSavedMovePtr AllocateNewMove() override;
};
