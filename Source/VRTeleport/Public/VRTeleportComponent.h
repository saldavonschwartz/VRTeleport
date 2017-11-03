//    The MIT License (MIT) 
// 
//    Copyright (c) 2017 Federico Saldarini 
//    https://www.linkedin.com/in/federicosaldarini 
//    https://github.com/saldavonschwartz 
//    http://0xfede.io 
// 
// 
//    Permission is hereby granted, free of charge, to any person obtaining a copy 
//    of this software and associated documentation files (the "Software"), to deal 
//    in the Software without restriction, including without limitation the rights 
//    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
//    copies of the Software, and to permit persons to whom the Software is 
//    furnished to do so, subject to the following conditions: 
// 
//    The above copyright notice and this permission notice shall be included in all 
//    copies or substantial portions of the Software. 
// 
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
//    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
//    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
//    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
//    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
//    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
//    SOFTWARE. 

#pragma once

#include "Private/MKTickerTask.h"
#include "Components/SceneComponent.h"
#include "VRTeleportComponent.generated.h"

UENUM(BlueprintType)
enum class EVRProbeMode : uint8
{
	Linear,
	Projectile
};

USTRUCT(BlueprintType)
struct FVRProbeInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
		bool HitSomething;

	UPROPERTY(BlueprintReadWrite)
		bool GroundHit;

	UPROPERTY(BlueprintReadWrite)
		bool IsHitValid;

	UPROPERTY(BlueprintReadWrite)
		FHitResult Hit;

	UPROPERTY(BlueprintReadWrite)
		TArray<FPredictProjectilePathPointData> PathPoints;
};

UCLASS()
class VRTELEPORT_API UVRProbeResponse : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
		FVRProbeInfo ProbeInfo;

	UPROPERTY(BlueprintReadWrite)
		bool ShouldPerformDefaultImplementation;
};

UCLASS()
class VRTELEPORT_API UVRTeleportResponse : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
		FVector TargetLocation;

	UPROPERTY(BlueprintReadWrite)
		FVector ActorLocation;

	UPROPERTY(BlueprintReadWrite)
		bool ShouldPerformDefaultImplementation;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRTeleportD1, UVRProbeResponse*, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRTeleportD2, UVRTeleportResponse*, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRTeleportD3, FVector, ActorLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVRTeleportD4);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRTELEPORT_API UVRTeleportComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
		FVRTeleportD4 OnStartProbing;

	UPROPERTY(BlueprintAssignable)
		FVRTeleportD1 OnProbing;

	UPROPERTY(BlueprintAssignable)
		FVRTeleportD4 OnEndProbing;

	UPROPERTY(BlueprintAssignable)
		FVRTeleportD2 OnStartTeleporting;

	UPROPERTY(BlueprintAssignable)
		FVRTeleportD3 OnEndTeleporting;

	UPROPERTY(EditAnyWhere)
		float ProbeDistance = 500;

	UPROPERTY(EditAnyWhere)
		float TeleportTravelTime = .15;

	UFUNCTION(BlueprintCallable)
		FVector GetLastTeleportLocation() const;

	UFUNCTION(BlueprintCallable)
		FVRProbeInfo GetLastProbeInfo() const;

	UFUNCTION(BlueprintCallable)
		USceneComponent* GetMarker() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
		void SetProbeMode(EVRProbeMode Mode);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
		void StartProbing();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
		void EndProbing();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
		bool TeleportToLastProbedLocation();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
		void TeleportToLocation(FVector Location);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
		void SetActorsToIgnore(TArray<AActor*> Actors);

	UVRTeleportComponent();

protected:
	void BeginPlay() override;

private:
	struct VRTraceQuery
	{
		struct
		{
			FCollisionQueryParams Parameters;
			FCollisionObjectQueryParams ObjectParameters;
		} Linear;

		struct
		{
			FPredictProjectilePathParams Parameters;
		} Projectile;

	};

	UPROPERTY(EditDefaultsOnly)
		EVRProbeMode ProbeMode = EVRProbeMode::Projectile;

	UPROPERTY(ReplicatedUsing = OnRep_IsProbing)
		bool IsProbing = false;

	UPROPERTY()
		UVRProbeResponse* ProbeResponse;

	UPROPERTY()
		UVRTeleportResponse* TeleportResponse;

	USceneComponent* Marker;
	MKTickerTask TeleportProbingTask;
	MKTickerTask TeleportTask;
	VRTraceQuery TraceQuery;
	TArray<AActor*> ActorsToIgnore;
	bool ValidTeleportLocation;
	float TeleportToLocationProgress;

	UFUNCTION()
		void OnRep_IsProbing();

	void Teleport();
	void UpdateMarker();
	void UpdateTeleportLocation();
	bool LinearProbe(float Delta);
	bool ProjectileProbe(float Delta);
};