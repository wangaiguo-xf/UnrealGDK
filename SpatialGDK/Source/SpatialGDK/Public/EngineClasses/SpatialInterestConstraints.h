// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "Containers/Array.h"
#include "Math/Vector.h"

#include "SpatialInterestConstraints.generated.h"

namespace SpatialGDK
{
struct Query;
struct QueryConstraint;
}
class USchemaDatabase;

UCLASS(Abstract, BlueprintInternalUseOnly)
class SPATIALGDK_API UAbstractQueryConstraint : public UObject
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const PURE_VIRTUAL(UAbstractQueryConstraint::CreateConstraint, );
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UOrConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Instanced)
	TArray<UAbstractQueryConstraint *> Constraints;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UAndConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Instanced)
	TArray<UAbstractQueryConstraint *> Constraints;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API USphereConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FVector Center;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	float Radius;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UCylinderConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FVector Center;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	float Radius;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UBoxConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FVector Center;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	FVector EdgeLengths;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API URelativeSphereConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	float Radius;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API URelativeCylinderConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	float Radius;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API URelativeBoxConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	FVector EdgeLengths;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UCheckoutRadiusConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Meta = (ClampMin = 0.0))
	float Radius;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<AActor> ActorClass;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UActorClassConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<AActor> ActorClass;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class SPATIALGDK_API UComponentClassConstraint : public UAbstractQueryConstraint
{
	GENERATED_BODY()
public:
	virtual void CreateConstraint(const USchemaDatabase& SchemaDatabase, SpatialGDK::QueryConstraint& OutConstraint) const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<UActorComponent> ComponentClass;
};






