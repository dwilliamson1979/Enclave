// Copyright Lightcarrier Studios

#include "Animation/EnclaveCharacterAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UEnclaveCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	TryRefreshPawnBinding();
}

// Game thread: the only phase allowed to touch UObjects or call interfaces.
void UEnclaveCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);


	// No pawn (editor preview, mid-teardown) or a pawn that doesn't provide
	// anim data: fall back to a neutral pose rather than stale values.
	if (!TryRefreshPawnBinding())
	{
		ResetAnimationState();
		return;
	}

	AnimData = AnimDataProvider->GetAnimData();

	TryBindGameplayTagPropertyMap(CachedPawn.Get());
}

// Worker thread (or game thread if multithreaded anim update is disabled).
// Runs after NativeUpdateAnimation and before the AnimGraph evaluates.
// Only reads/writes this instance's own members.
void UEnclaveCharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const FVector2D PlanarLocalVelocity(AnimData.LocalVelocity.X, AnimData.LocalVelocity.Y);

	GroundSpeed = PlanarLocalVelocity.Size();
	bIsMoving = GroundSpeed > MoveSpeedThreshold;

	MoveDirection = bIsMoving
		? FMath::RadiansToDegrees(FMath::Atan2(PlanarLocalVelocity.Y, PlanarLocalVelocity.X))
		: 0.0f;
}

bool UEnclaveCharacterAnimInstance::TryRefreshPawnBinding()
{
	APawn* Pawn = TryGetPawnOwner();

	// Also catches a destroyed pawn: the weak pointer reads null, so it
	// differs from a live new pawn (or matches a null one, and we bail below).
	if (CachedPawn.Get() != Pawn)
	{
		CachedPawn = Pawn;
		AnimDataProvider = Cast<IEnclaveAnimDataProviderInterface>(Pawn);

		// A different pawn means a different (or no) ASC - rebind on next update.
		BoundAbilitySystemComponent.Reset();
	}

	return Pawn && AnimDataProvider;
}

void UEnclaveCharacterAnimInstance::TryBindGameplayTagPropertyMap(APawn* Pawn)
{
	if (BoundAbilitySystemComponent.IsValid())
	{
		return;
	}

	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Pawn);
	UAbilitySystemComponent* ASC = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;

	if (!ASC)
	{
		// PlayerState (and therefore the ASC) hasn't arrived yet - retry next frame.
		return;
	}

	// Registers tag-change callbacks and applies the tags' current counts
	// immediately, so a pawn that spawns mid-reload starts in the right state.
	GameplayTagPropertyMap.Initialize(this, ASC);
	BoundAbilitySystemComponent = ASC;
}

void UEnclaveCharacterAnimInstance::ResetAnimationState()
{
	AnimData = FEnclaveAnimData();
}

#if WITH_EDITOR
EDataValidationResult UEnclaveCharacterAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);

	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
