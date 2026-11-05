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

	TryBindGameplayTagBindings(CachedPawn.Get());
}

void UEnclaveCharacterAnimInstance::NativeUninitializeAnimation()
{
	// The pawn is going (or this layer is being unlinked), but the ASC lives on
	// and keeps sending tag changes: unbind now, not at garbage collection.
	GameplayTagBindings.Unbind();
	CachedPawn.Reset();
	AnimDataProvider = nullptr;

	Super::NativeUninitializeAnimation();
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

		// A different pawn means a different (or no) ASC: drop the old binding
		// now; TryBindGameplayTagBindings binds the new one.
		GameplayTagBindings.Unbind();
	}

	return Pawn && AnimDataProvider;
}

void UEnclaveCharacterAnimInstance::TryBindGameplayTagBindings(APawn* Pawn)
{
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Pawn);
	UAbilitySystemComponent* ASC = AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;

	// Bound to this ASC already (the usual case), or no ASC yet because the
	// PlayerState hasn't arrived: nothing to do, check again next frame.
	if (!ASC || ASC == GameplayTagBindings.GetBoundAbilitySystemComponent())
	{
		return;
	}

	// First bind, or a new ASC (a new PlayerState). Bind unbinds the old one,
	// and applies current counts so a pawn that spawns mid-reload starts in
	// the right state.
	GameplayTagBindings.Bind(this, ASC);
}

void UEnclaveCharacterAnimInstance::ResetAnimationState()
{
	AnimData = FEnclaveAnimData();
}

#if WITH_EDITOR
EDataValidationResult UEnclaveCharacterAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagBindings.ValidateBindings(GetClass(), Context);

	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
