#pragma once

#include "CoreMinimal.h"
#include "Camera/Feedback/WuwaCameraFeedbackTypes.h"
#include "UObject/StrongObjectPtr.h"

/** 多来源短时镜头反馈的唯一生命周期与混合状态拥有者 */
class WUWA_API FWuwaCameraFeedbackStack
{
public:
	/**
     * 取得一次镜头反馈
     * @param Spec	本次反馈的值配置
     * @param SourceObject	反馈来源弱引用
     * @return 成功时返回唯一 Handle
     */
	FWuwaCameraFeedbackRequestHandle AcquireFeedback(const FWuwaCameraFeedbackSpec& Spec, UObject* SourceObject);

	/**
     * 让活动反馈进入 Released Tail
     * @param Handle	目标反馈 Handle
     * @return 是否找到并推进目标反馈
     */
	bool BeginReleaseFeedback(const FWuwaCameraFeedbackRequestHandle& Handle);

	/**
     * 立即移除指定反馈
     * @param Handle	目标反馈 Handle
     * @return 是否移除目标反馈
     */
	bool ForceReleaseFeedback(const FWuwaCameraFeedbackRequestHandle& Handle);

	/**
     * 立即移除来源持有的全部反馈
     * @param SourceObject	目标来源
     * @return 移除数量
     */
	int32 ForceReleaseBySource(const UObject* SourceObject);

	/**
     * 推进生命周期并分别混合各通道
     * @param DeltaTime	本帧时长
     * @return 当前只读反馈输出
     */
	const FWuwaCameraFeedbackOutput& Update(float DeltaTime);

	/** 清空所有反馈与序号 */
	void Reset();

	/** @return 当前存储的反馈数量 */
	int32 GetEntryCount() const
	{
		return Entries.Num();
	}

	/**
     * 查询指定反馈的当前生命周期
     * @param Handle	目标反馈 Handle
     * @return 不存在时返回 Removed
     */
	EWuwaCameraFeedbackLifecycle GetLifecycle(const FWuwaCameraFeedbackRequestHandle& Handle) const;

	/** @return 最近一次混合结果 */
	const FWuwaCameraFeedbackOutput& GetOutput() const
	{
		return Output;
	}

private:
	/** 单个来源的内部生命周期记录 */
	struct FEntry
	{
		FWuwaCameraFeedbackRequestHandle Handle;
		FWuwaCameraFeedbackSpec Spec;
		TStrongObjectPtr<UCurveFloat> BlendInCurve;
		TStrongObjectPtr<UCurveFloat> BlendOutCurve;
		TWeakObjectPtr<UObject> SourceObject;
		EWuwaCameraFeedbackLifecycle Lifecycle = EWuwaCameraFeedbackLifecycle::Active;
		float ElapsedTime = 0.f;
		float ReleaseStartWeight = 1.f;
	};

	/** @return 指定记录当前的混合权重 */
	static float EvaluateWeight(const FEntry& Entry);

	TArray<FEntry> Entries;
	FWuwaCameraFeedbackOutput Output;
	int64 NextHandleValue = 1;
};
