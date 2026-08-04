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
    All UMETA(DisplayName = "Movement + Targeting")
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

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction *ThisTickFunction) override;

private:
    /** 注册当前本地视口的 Canvas 绘制回调。 */
    void RegisterCanvasDelegate();

    /**
     * 对称清理注册句柄。
     * PIE 退出和 Owner 销毁都必须经过这里，避免遗留失效回调。
     */
    void UnregisterCanvasDelegate();

    /** UE Debug Draw Service 在当前视口渲染时调用。 */
    void DrawDebugCanvas(UCanvas *Canvas, APlayerController *PlayerController);

    /** 绘制 Movement、Router、FIFO 与 State Tags 的屏幕事实。 */
    void DrawMovementCanvas(
        UCanvas &Canvas,
        UFont &Font,
        APlayerController &PlayerController,
        float &InOutY) const;

    /** 绘制角色 Capsule、朝向、速度与相机相对输入方向。 */
    void DrawMovementWorld(const AWuwaCharacter &Character) const;

    /** 绘制 Target Context、评分与最后失败原因。 */
    void DrawTargetingCanvas(
        UCanvas &Canvas,
        UFont &Font,
        APlayerController &PlayerController,
        float &InOutY) const;

    /** 绘制当前目标点、角色到目标和当前相机视线方向。 */
    void DrawTargetingWorld(
        const AWuwaCharacter &Character,
        const APlayerController &PlayerController) const;

    /** 把 Target Context 的目标点投影成屏幕十字，不参与目标选择。 */
    void DrawTargetScreenMarker(
        UCanvas &Canvas,
        APlayerController &PlayerController,
        const AWuwaCharacter &Character) const;

    /** 仅从 Controller 当前 Pawn 解析 Wuwa Character，不搜索世界。 */
    AWuwaCharacter *ResolveCharacter(const APlayerController &PlayerController) const;

    static void DrawCanvasLine(
        UCanvas &Canvas,
        UFont &Font,
        const FString &Text,
        const FColor &Color,
        float &InOutY);

    static bool IncludesMovement(EWuwaDebugVisualizationMode Mode);

    static bool IncludesTargeting(EWuwaDebugVisualizationMode Mode);

    static bool RequiresWorldTick(EWuwaDebugVisualizationMode Mode);

    static const TCHAR *GetModeDisplayName(EWuwaDebugVisualizationMode Mode);

    UPROPERTY(VisibleInstanceOnly, Transient, Category = "Debug")
    EWuwaDebugVisualizationMode VisualizationMode = EWuwaDebugVisualizationMode::Disabled;

    /** 只代表本组件注册的 Canvas 回调，不能被其他对象释放。 */
    FDelegateHandle CanvasDelegateHandle;
};
