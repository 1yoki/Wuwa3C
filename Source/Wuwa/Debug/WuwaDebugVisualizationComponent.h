#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "WuwaDebugVisualizationComponent.generated.h"

class APlayerController;
class AWuwaCharacter;
class UCanvas;
class UFont;

/**
 * 演示 Debug 叠层当前显示的内容。
 *
 * 该枚举只控制只读可视化，不允许改变 Gameplay 状态。
 */
UENUM(BlueprintType)
enum class EWuwaDebugVisualizationMode : uint8
{
	Disabled UMETA(DisplayName = "关闭"),
	Movement UMETA(DisplayName = "Movement"),
	Targeting UMETA(DisplayName = "Targeting"),

	/** Camera Mode、Rig、碰撞与构图 */
	Camera UMETA(DisplayName = "Camera"),

	All UMETA(DisplayName = "Movement + Targeting + Camera")
};

/**
 * 演示视频使用的只读 Debug Visualization。
 *
 * Component 只消费 Gameplay 系统公开的只读事实；不拥有运行状态，
 * 也不得重新执行目标搜索或修改角色、动作、移动与镜头行为。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaDebugVisualizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaDebugVisualizationComponent();

	/** 按固定顺序切换：关闭 → Movement → Targeting → 全部 → 关闭。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Debug")
	void CycleVisualizationMode();

	/** 显式设置显示模式，只改变 Debug 表现。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Debug")
	void SetVisualizationMode(EWuwaDebugVisualizationMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Wuwa|Debug")
	EWuwaDebugVisualizationMode GetVisualizationMode() const
	{
		return VisualizationMode;
	}

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void
	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 注册当前本地视口的 Canvas 绘制回调。 */
	void RegisterCanvasDelegate();

	/**
     * 对称清理注册句柄。
     * PIE 退出和 Owner 销毁都必须经过这里，避免遗留失效回调。
     */
	void UnregisterCanvasDelegate();

	/** UE Debug Draw Service 在当前视口渲染时调用。 */
	void DrawDebugCanvas(UCanvas* Canvas, APlayerController* PlayerController);

	/**
     * 绘制综合模式的精简屏幕摘要
     *
     * @param Canvas			当前视口画布
     * @param Font				调试字体
     * @param PlayerController	本地玩家控制器
     * @param InOutY			下一行文字的纵坐标
     */
	void DrawAllCanvasSummary(UCanvas& Canvas, UFont& Font, APlayerController& PlayerController, float& InOutY) const;

	/**
     * 绘制综合模式保留的角色胶囊、目标框与镜头双球
     *
     * @param Character			当前玩家角色
     * @param PlayerController	本地玩家控制器
     */
	void DrawAllWorldSummary(const AWuwaCharacter& Character, const APlayerController& PlayerController) const;

	/** 绘制 Movement、Router、FIFO 与 State Tags 的屏幕事实。 */
	void DrawMovementCanvas(UCanvas& Canvas, UFont& Font, APlayerController& PlayerController, float& InOutY) const;

	/**
     * 绘制本地 Pawn 的网络角色与控制关系
     *
     * @param Canvas		当前视口画布
     * @param Font		调试字体
     * @param Character	当前玩家角色
     * @param InOutY		下一行文本的纵坐标
     */
	void DrawNetworkSummary(UCanvas& Canvas, UFont& Font, const AWuwaCharacter& Character, float& InOutY) const;

	/**
     * 绘制能力系统和基础战斗属性只读摘要
     *
     * @param Canvas			当前视口画布
     * @param Font			调试字体
     * @param Character		当前玩家角色
     * @param InOutY			下一行文本的纵坐标
     */
	void DrawAbilitySystemSummary(UCanvas& Canvas, UFont& Font, const AWuwaCharacter& Character, float& InOutY) const;

	/**
     * 绘制输入路由最近一次 Ability 激活诊断
     *
     * @param Canvas		当前视口画布
     * @param Font		调试字体
     * @param Character	当前玩家角色
     * @param InOutY		下一行文本的纵坐标
     */
	void DrawAbilityInputSummary(UCanvas& Canvas, UFont& Font, const AWuwaCharacter& Character, float& InOutY) const;

	/**
     * 绘制战斗属性、最近伤害与死亡状态
     *
     * @param Canvas		当前视口画布
     * @param Font		调试字体
     * @param Character	当前玩家角色
     * @param InOutY		下一行文本的纵坐标
     */
	void DrawHealthSummary(UCanvas& Canvas, UFont& Font, const AWuwaCharacter& Character, float& InOutY) const;

	/**
     * 绘制武器装配和近战攻击生命周期只读摘要
     *
     * @param Canvas		当前视口画布
     * @param Font		调试字体
     * @param Character	当前玩家角色
     * @param InOutY		下一行文本的纵坐标
     */
	void DrawCombatSummary(UCanvas& Canvas, UFont& Font, const AWuwaCharacter& Character, float& InOutY) const;

	/**
     * 绘制当前权威剑刃轨迹和轨迹半径
     *
     * @param Character	当前玩家角色
     * @return 无
     */
	void DrawCombatExecutionWorld(const AWuwaCharacter& Character) const;

	/** 绘制角色 Capsule、朝向、速度与相机相对输入方向。 */
	void DrawMovementWorld(const AWuwaCharacter& Character) const;

	/** 绘制 Target Context、评分与最后失败原因。 */
	void DrawTargetingCanvas(UCanvas& Canvas, UFont& Font, APlayerController& PlayerController, float& InOutY) const;

	/** 绘制当前目标球与目标 Actor 包围盒 */
	void DrawTargetingWorld(const AWuwaCharacter& Character) const;

	/** 绘制 Camera Mode、Rig、SpringArm 与构图的屏幕事实 */
	void DrawCameraCanvas(UCanvas& Canvas, UFont& Font, APlayerController& PlayerController, float& InOutY) const;

	/**
     * 绘制 Pivot 与画面中心两个世界空间线框球
     *
     * @param Character			当前玩家角色
     * @param PlayerController	本地玩家控制器
     */
	void DrawCameraCompositionSpheres(const AWuwaCharacter& Character, const APlayerController& PlayerController) const;

	/**
     * 将固定画幅半径换算为指定深度下的世界半径
     *
     * @param HorizontalFieldOfView	当前水平视场角
     * @param ViewportWidth			视口宽度
     * @param ViewportHeight			视口高度
     * @param ViewDepth				球心沿镜头前向的深度
     * @param OutWorldRadius			输出的世界空间半径
     * @return 输入有效且成功得到有限正半径时返回 true
     */
	static bool CalculateScreenSpaceSphereRadius(
	    float HorizontalFieldOfView, int32 ViewportWidth, int32 ViewportHeight, float ViewDepth, float& OutWorldRadius);

	/**
     * 解析角色模型颈部锚点的世界坐标
     *
     * @param Character		当前玩家角色
     * @param OutAnchorLocation	输出的锚点世界坐标
     * @return 角色模型和 neck_01 锚点有效时返回 true
     */
	static bool ResolveCharacterMeshAnchor(const AWuwaCharacter& Character, FVector& OutAnchorLocation);

	/** 仅从 Controller 当前 Pawn 解析 Wuwa Character，不搜索世界。 */
	AWuwaCharacter* ResolveCharacter(const APlayerController& PlayerController) const;

	static void DrawCanvasLine(UCanvas& Canvas, UFont& Font, const FString& Text, const FColor& Color, float& InOutY);

	static bool IncludesMovement(EWuwaDebugVisualizationMode Mode);

	static bool IncludesTargeting(EWuwaDebugVisualizationMode Mode);

	/** @return 当前模式是否包含 Camera 诊断 */
	static bool IncludesCamera(EWuwaDebugVisualizationMode Mode);

	static bool RequiresWorldTick(EWuwaDebugVisualizationMode Mode);

	static const TCHAR* GetModeDisplayName(EWuwaDebugVisualizationMode Mode);

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Debug")
	EWuwaDebugVisualizationMode VisualizationMode = EWuwaDebugVisualizationMode::Disabled;

	/** 只代表本组件注册的 Canvas 回调，不能被其他对象释放。 */
	FDelegateHandle CanvasDelegateHandle;

	/** 下次允许输出镜头双球无效数据警告的世界时间 */
	mutable double NextCameraCompositionWarningTime = 0.0;
};
