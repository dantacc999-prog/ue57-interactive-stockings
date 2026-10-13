#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "StockingsStandingPose.generated.h"

// An isolated, static fitting pose. The primary animation instance and its gameplay state remain installed.
UCLASS(Transient)
class STOCKINGSINTERACTION_API UStockingsStandingPose : public UAnimInstance {
 GENERATED_BODY()
public:
 TArray<FTransform> StandingLocalPose;
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
