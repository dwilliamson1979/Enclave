// Copyright Lightcarrier Studios

#include "Combat/EnclaveHistoricalCapsuleIntersection.h"

#include "Combat/Components/EnclaveTargetHistoryComponent.h"

bool FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(const FVector& Start, const FVector& End,
	const FEnclaveTargetCapsuleFrame& Capsule, double& OutDistanceCm, FVector& OutPoint)
{
	OutDistanceCm = 0.0;
	OutPoint = FVector::ZeroVector;
	if (Start.ContainsNaN() || End.ContainsNaN() || Capsule.Center.ContainsNaN() ||
		Capsule.Rotation.ContainsNaN() || !FMath::IsFinite(Capsule.RadiusCm) ||
		!FMath::IsFinite(Capsule.HalfHeightCm) || Capsule.RadiusCm <= 0.0f ||
		Capsule.HalfHeightCm < Capsule.RadiusCm)
	{
		return false;
	}

	const FVector Segment = End - Start;
	const double SegmentLength = Segment.Size();
	if (!FMath::IsFinite(SegmentLength) || SegmentLength <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector Direction = Segment / SegmentLength;
	const FVector Axis = Capsule.Rotation.GetNormalized().GetAxisZ();
	const double AxisHalfLength = static_cast<double>(Capsule.HalfHeightCm - Capsule.RadiusCm);
	const double Radius = Capsule.RadiusCm;
	const FVector AxisStart = Capsule.Center - Axis * AxisHalfLength;
	const FVector AxisEnd = Capsule.Center + Axis * AxisHalfLength;

	// If the trace starts inside the solid capsule, its entry distance is zero.
	const FVector ToStart = Start - AxisStart;
	const double AxisFraction = AxisHalfLength > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp(FVector::DotProduct(ToStart, Axis) / (2.0 * AxisHalfLength), 0.0, 1.0)
		: 0.0;
	const FVector ClosestAxisPoint = FMath::Lerp(AxisStart, AxisEnd, AxisFraction);
	if (FVector::DistSquared(Start, ClosestAxisPoint) <= Radius * Radius)
	{
		OutPoint = Start;
		return true;
	}

	double NearestEntry = SegmentLength + 1.0;
	const FVector FromCenter = Start - Capsule.Center;
	const double StartHeight = FVector::DotProduct(FromCenter, Axis);
	const double DirectionHeight = FVector::DotProduct(Direction, Axis);
	const FVector RadialStart = FromCenter - Axis * StartHeight;
	const FVector RadialDirection = Direction - Axis * DirectionHeight;

	// Infinite cylinder intersection, clipped to the finite central section.
	// A trace parallel to the axis is handled by the two spherical caps below.
	if (AxisHalfLength > UE_DOUBLE_SMALL_NUMBER)
	{
		const double A = FVector::DotProduct(RadialDirection, RadialDirection);
		if (A > 1.0e-12)
		{
			const double B = FVector::DotProduct(RadialStart, RadialDirection);
			const double C = FVector::DotProduct(RadialStart, RadialStart) - Radius * Radius;
			const double Discriminant = B * B - A * C;
			if (Discriminant >= 0.0)
			{
				const double Entry = (-B - FMath::Sqrt(Discriminant)) / A;
				const double Height = StartHeight + Entry * DirectionHeight;
				if (Entry >= 0.0 && Entry <= SegmentLength && FMath::Abs(Height) <= AxisHalfLength)
				{
					NearestEntry = Entry;
				}
			}
		}
	}

	// The union of the central cylinder and endpoint spheres is the capsule.
	// Full endpoint spheres are safe: their inner hemispheres lie inside the
	// cylinder (or other endpoint sphere), so min(entry) stays correct.
	for (const FVector& SphereCenter : { AxisStart, AxisEnd })
	{
		const FVector ToSphere = Start - SphereCenter;
		const double B = FVector::DotProduct(ToSphere, Direction);
		const double C = FVector::DotProduct(ToSphere, ToSphere) - Radius * Radius;
		const double Discriminant = B * B - C;
		if (Discriminant >= 0.0)
		{
			const double Entry = -B - FMath::Sqrt(Discriminant);
			if (Entry >= 0.0 && Entry <= SegmentLength)
			{
				NearestEntry = FMath::Min(NearestEntry, Entry);
			}
		}
	}

	if (NearestEntry > SegmentLength)
	{
		return false;
	}

	OutDistanceCm = NearestEntry;
	OutPoint = Start + Direction * NearestEntry;
	return true;
}

FVector FEnclaveHistoricalCapsuleIntersection::SurfaceNormal(const FEnclaveTargetCapsuleFrame& Capsule,
	const FVector& Point, const FVector& FallbackNormal)
{
	const FVector Axis = Capsule.Rotation.GetNormalized().GetAxisZ();
	const double AxisHalfLength = FMath::Max(0.0, static_cast<double>(Capsule.HalfHeightCm - Capsule.RadiusCm));
	const double AlongAxis = FMath::Clamp(FVector::DotProduct(Point - Capsule.Center, Axis), -AxisHalfLength, AxisHalfLength);
	const FVector Normal = (Point - (Capsule.Center + Axis * AlongAxis)).GetSafeNormal();
	return Normal.IsNearlyZero() ? FallbackNormal.GetSafeNormal() : Normal;
}
