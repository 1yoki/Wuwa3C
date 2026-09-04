#pragma once

#include "CoreMinimal.h"
#include "Movement/Actions/WuwaMovementActionTypes.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"

/** SavedMove 本地保存的普通 MovementAction 最小物理重演状态 */
struct WUWA_API FWuwaMovementActionNetworkReplayState
{
	/** 当前物理动作所属跨端序号 */
	FWuwaNetworkActionGeneration Generation;

	/** 最近已经原子应用的退出序号 */
	FWuwaNetworkActionGeneration LastAppliedExitGeneration;

	/** 当前物理动作标签 */
	FGameplayTag ActionTag;

	/** 完成出口使用的事实输入 */
	FVector2D ExitMoveIntent = FVector2D::ZeroVector;

	/** 当前物理动作使用的移动驱动 */
	EWuwaMovementActionDriver Driver = EWuwaMovementActionDriver::RootMotionSource;

	/** 当前物理动作完成后的出口策略 */
	EWuwaMovementActionExitPolicy ExitPolicy = EWuwaMovementActionExitPolicy::None;

	/** 当前 Move 起点是否持有普通 MovementAction 物理状态 */
	bool bMovementActionActive = false;

	/** 当前 Move 起点是否持有动作朝向覆盖 */
	bool bFacingOverrideActive = false;

	/** 当前 RMS 退出时是否保留末速度 */
	bool bPreserveVelocityOnRelease = false;

	/** 朝向覆盖前是否使用移动方向 */
	bool bSavedOrientRotationToMovement = false;

	/** 朝向覆盖前是否使用 Controller Desired Rotation */
	bool bSavedUseControllerDesiredRotation = false;

	/** 朝向覆盖前角色是否使用 Controller Yaw */
	bool bSavedUseControllerRotationYaw = false;

	/** 清空全部本地重演字段 */
	void Reset()
	{
		*this = FWuwaMovementActionNetworkReplayState();
	}

	/** @return 当前状态是否可以安全参与 SavedMove 重演 */
	bool IsPayloadValid() const
	{
		const bool bFiniteIntent = FMath::IsFinite(ExitMoveIntent.X) && FMath::IsFinite(ExitMoveIntent.Y);
		const bool bValidDriver =
		    static_cast<uint8>(Driver) <= static_cast<uint8>(EWuwaMovementActionDriver::RootMotionSource);
		const bool bValidExitPolicy =
		    static_cast<uint8>(ExitPolicy) <= static_cast<uint8>(EWuwaMovementActionExitPolicy::TryEnterSprintRun);
		if (!bFiniteIntent || ExitMoveIntent.GetAbsMax() > 1.001f || !bValidDriver || !bValidExitPolicy)
		{
			return false;
		}

		if (bMovementActionActive)
		{
			return Generation.IsValid() && ActionTag.IsValid();
		}

		return !Generation.IsValid() && !ActionTag.IsValid() && !bFacingOverrideActive && !bPreserveVelocityOnRelease &&
		       ExitPolicy == EWuwaMovementActionExitPolicy::None && ExitMoveIntent.IsNearlyZero();
	}

	/**
     * 比较两个历史 Move 的全部物理状态
     * @param Other	待比较的 MovementAction 重演状态
     * @return 两个 Move 是否允许继续参与原生合并判断
     */
	bool IsEquivalentForCombine(const FWuwaMovementActionNetworkReplayState& Other) const
	{
		return Generation == Other.Generation && LastAppliedExitGeneration == Other.LastAppliedExitGeneration &&
		       ActionTag == Other.ActionTag && ExitMoveIntent.Equals(Other.ExitMoveIntent, KINDA_SMALL_NUMBER) &&
		       Driver == Other.Driver && ExitPolicy == Other.ExitPolicy &&
		       bMovementActionActive == Other.bMovementActionActive &&
		       bFacingOverrideActive == Other.bFacingOverrideActive &&
		       bPreserveVelocityOnRelease == Other.bPreserveVelocityOnRelease &&
		       bSavedOrientRotationToMovement == Other.bSavedOrientRotationToMovement &&
		       bSavedUseControllerDesiredRotation == Other.bSavedUseControllerDesiredRotation &&
		       bSavedUseControllerRotationYaw == Other.bSavedUseControllerRotationYaw;
	}
};
