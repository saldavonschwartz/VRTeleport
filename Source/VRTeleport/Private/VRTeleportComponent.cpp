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

#include "VRTeleportComponent.h"
#include "UnrealNetwork.h"
#include "Kismet/KismetMathLibrary.h"

#define MK_IS_SERVER \
(GetWorld() && GetWorld()->GetNetMode() != NM_Client)

#define MK_ASSERT_SERVER \
if (!MK_IS_SERVER) { \
	UE_LOG(LogTemp, Error, TEXT("Called from client. (Marked as MK_ASSERT_SERVER)")); \
	check(MK_IS_SERVER); \
}

UVRTeleportComponent::UVRTeleportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetComponentTickEnabled(false);

	TeleportResponse = CreateDefaultSubobject<UVRTeleportResponse>(TEXT("TeleportResponse"));
	ProbeResponse = CreateDefaultSubobject<UVRProbeResponse>(TEXT("ProbeResponse"));
}

void UVRTeleportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVRTeleportComponent, IsProbing);
}

void UVRTeleportComponent::BeginPlay()
{
	ActorsToIgnore = { GetOwner() };

	TraceQuery.Linear.ObjectParameters.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
	TraceQuery.Linear.ObjectParameters.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

	TraceQuery.Linear.Parameters.bTraceAsyncScene = true;
	TraceQuery.Linear.Parameters.AddIgnoredActors(ActorsToIgnore);

	auto Static = UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic);
	auto Dynamic = UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic);
	auto Pawn = UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn);
	auto Physics = UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody);

	TraceQuery.Projectile.Parameters.ObjectTypes = { Static, Dynamic, Pawn, Physics };
	TraceQuery.Projectile.Parameters.ActorsToIgnore = ActorsToIgnore;
	TraceQuery.Projectile.Parameters.MaxSimTime = 2.;
	TraceQuery.Projectile.Parameters.SimFrequency = 15.;
	TraceQuery.Projectile.Parameters.bTraceWithCollision = true;
	TraceQuery.Projectile.Parameters.ProjectileRadius = 0;
	TraceQuery.Projectile.Parameters.DrawDebugType = EDrawDebugTrace::ForOneFrame;

	Marker = GetChildComponent(0);

	if (Marker) {
		Marker->SetVisibility(false);
		Marker->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	SetProbeMode(ProbeMode);
	Super::BeginPlay();
}

FVector UVRTeleportComponent::GetLastTeleportLocation() const
{
	return TeleportResponse->ActorLocation;
}

FVRProbeInfo UVRTeleportComponent::GetLastProbeInfo() const
{
	return ProbeResponse->ProbeInfo;
}

USceneComponent* UVRTeleportComponent::GetMarker() const
{
	return Marker;
}

void UVRTeleportComponent::SetActorsToIgnore(TArray<AActor*> Actors)
{
	MK_ASSERT_SERVER;

	ActorsToIgnore = Actors;
	ActorsToIgnore.AddUnique(GetOwner());

	if (ProbeMode == EVRProbeMode::Linear) {
		TraceQuery.Linear.Parameters.AddIgnoredActors(ActorsToIgnore);
	}
	else {
		TraceQuery.Projectile.Parameters.ActorsToIgnore = ActorsToIgnore;
	}
}

void UVRTeleportComponent::SetProbeMode(EVRProbeMode Mode)
{
	MK_ASSERT_SERVER;

	ProbeMode = Mode;

	if (IsProbing) {
		TeleportProbingTask.End();
	}

	if (ProbeMode == EVRProbeMode::Linear) {
		TraceQuery.Linear.Parameters.AddIgnoredActors(ActorsToIgnore);
		TeleportProbingTask.Delegate = FTickerDelegate::CreateUObject(
			this,
			&UVRTeleportComponent::LinearProbe
		);
	}
	else {
		TraceQuery.Projectile.Parameters.ActorsToIgnore = ActorsToIgnore;
		TeleportProbingTask.Delegate = FTickerDelegate::CreateUObject(
			this,
			&UVRTeleportComponent::ProjectileProbe
		);
	}

	if (IsProbing) {
		TeleportProbingTask.Start();
	}
}

bool UVRTeleportComponent::LinearProbe(float Delta)
{
	FHitResult Hit;
	FVector TraceStart = GetComponentLocation();
	FVector TraceEnd = GetForwardVector() * ProbeDistance;
	auto& Info = ProbeResponse->ProbeInfo;

	Info.HitSomething = GetWorld()->LineTraceSingleByObjectType(
		Hit,
		TraceStart,
		TraceEnd,
		TraceQuery.Linear.ObjectParameters,
		TraceQuery.Linear.Parameters
	);

	if (Info.HitSomething) {
		FVector Up = { 0., 0., 1. };
		Info.GroundHit = ((Info.Hit.Normal | Up) >= 0.9);
		Info.IsHitValid = Info.GroundHit;
	}
	else {
		Info.IsHitValid = false;
	}

	Info.Hit = Hit;
	Info.PathPoints = {};
	ProbeResponse->ShouldPerformDefaultImplementation = true;

	OnProbing.Broadcast(ProbeResponse);
	
	if (ProbeResponse->ShouldPerformDefaultImplementation) {
		UpdateMarker();
	}

	return true;
}

bool UVRTeleportComponent::ProjectileProbe(float Delta)
{
	FPredictProjectilePathResult Results;
	FVector TraceStart = GetComponentLocation();
	FVector TraceEnd = GetForwardVector() * ProbeDistance;
	auto& Info = ProbeResponse->ProbeInfo;

	TraceQuery.Projectile.Parameters.StartLocation = TraceStart;
	TraceQuery.Projectile.Parameters.LaunchVelocity = TraceEnd;

	Info.HitSomething = UGameplayStatics::PredictProjectilePath(
		GetWorld(),
		TraceQuery.Projectile.Parameters,
		Results
	);

	if (Info.HitSomething) {
		FVector Up = { 0., 0., 1. };
		Info.GroundHit = ((Info.Hit.Normal | Up) >= 0.9);
		Info.IsHitValid = Info.GroundHit;
	}
	else {
		Info.IsHitValid = false;
	}

	Info.Hit = Results.HitResult;
	Info.PathPoints = Results.PathData;
	ProbeResponse->ShouldPerformDefaultImplementation = true;

	OnProbing.Broadcast(ProbeResponse);
	
	if (ProbeResponse->ShouldPerformDefaultImplementation) {
		UpdateMarker();
	}

	return true;
}

void UVRTeleportComponent::StartProbing()
{
	MK_ASSERT_SERVER;

	if (IsProbing) {
		return;
	}

	IsProbing = true;
	OnRep_IsProbing();
}

void UVRTeleportComponent::EndProbing()
{
	MK_ASSERT_SERVER;

	if (!IsProbing) {
		return;
	}

	IsProbing = false;
	OnRep_IsProbing();
}

bool UVRTeleportComponent::TeleportToLastProbedLocation()
{
	MK_ASSERT_SERVER;

	if (ProbeResponse->ProbeInfo.IsHitValid) {
		TeleportToLocation(ProbeResponse->ProbeInfo.Hit.Location);
	}

	return ProbeResponse->ProbeInfo.IsHitValid;
}

void UVRTeleportComponent::TeleportToLocation(FVector Location)
{
	MK_ASSERT_SERVER;

	auto CameraClass = UCameraComponent::StaticClass();
	auto Component = GetOwner()->GetComponentByClass(CameraClass);
	auto Camera = Cast<UCameraComponent>(Component);

	FVector CameraOffset = Camera->RelativeLocation;
	CameraOffset.Z = 0.;

	TeleportResponse->TargetLocation = Location;
	TeleportResponse->ActorLocation = Location - CameraOffset;
	TeleportResponse->ShouldPerformDefaultImplementation = true;

	OnStartTeleporting.Broadcast(TeleportResponse);
	
	if (TeleportResponse->ShouldPerformDefaultImplementation) {
		Teleport();
	}
	else {
		TeleportResponse->ActorLocation = GetOwner()->GetActorLocation();
		OnEndTeleporting.Broadcast(TeleportResponse->ActorLocation);
	}
}

void UVRTeleportComponent::Teleport()
{
	auto CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	CameraManager->StartCameraFade(1., 0., .25, FLinearColor::Black);

	TeleportToLocationProgress = 0.;

	TeleportTask = [this](float Delta) {
		TeleportToLocationProgress += (Delta / TeleportTravelTime);
		auto Owner = GetOwner();

		if (TeleportToLocationProgress < 1.) {
			auto LerpLocation = FMath::Lerp(
				Owner->GetActorLocation(),
				TeleportResponse->ActorLocation,
				TeleportToLocationProgress
			);

			Owner->SetActorLocation(LerpLocation);
		}
		else {
			Owner->SetActorLocation(TeleportResponse->ActorLocation);
			OnEndTeleporting.Broadcast(TeleportResponse->ActorLocation);
		}

		return (TeleportToLocationProgress < 1.);
	};

	TeleportTask.Start();
}

void UVRTeleportComponent::UpdateMarker()
{
	if (!Marker) {
		return;
	}

	auto& Info = ProbeResponse->ProbeInfo;

	if (!Info.IsHitValid) {
		Marker->SetVisibility(false);
		return;
	}

	FVector MarkerLocation = Info.Hit.Location;
	auto MarkerRotation = UKismetMathLibrary::MakeRotationFromAxes(Info.Hit.Normal, {}, {});

	Marker->SetWorldLocationAndRotation(MarkerLocation, MarkerRotation);
	Marker->SetVisibility(true);
}

void UVRTeleportComponent::OnRep_IsProbing()
{
	if (IsProbing) {
		OnStartProbing.Broadcast();
		TeleportProbingTask.Start();
	}
	else {
		TeleportProbingTask.End();

		if (Marker) {
			Marker->SetVisibility(false);
		}

		OnEndProbing.Broadcast();
	}
}
