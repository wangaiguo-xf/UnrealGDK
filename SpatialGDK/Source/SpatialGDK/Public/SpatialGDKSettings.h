#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Misc/Paths.h"

#include "SpatialGDKSettings.generated.h"

UCLASS(config = SpatialGDKSettings, defaultconfig)

class SPATIALGDK_API USpatialGDKSettings : public UObject
{
	GENERATED_BODY()

public:
	USpatialGDKSettings(const FObjectInitializer& ObjectInitializer);

	/** The number of entity IDs to be reserved when the entity pool is first created */
	UPROPERTY(EditAnywhere, config, Category = "Entity Pool", meta = (ConfigRestartRequired = false, DisplayName = "Initial Entity ID Reservation Count"))
	uint32 EntityPoolInitialReservationCount;


	/** The minimum number of entity IDs available in the pool before a new batch is reserved */
	UPROPERTY(EditAnywhere, config, Category = "Entity Pool", meta = (ConfigRestartRequired = false, DisplayName = "Pool Refresh Minimum Threshold"))
	uint32 EntityPoolRefreshThreshold;


	/** The number of entity IDs reserved when the minimum threshold is reached */
	UPROPERTY(EditAnywhere, config, Category = "Entity Pool", meta = (ConfigRestartRequired = false, DisplayName = "Refresh Count"))
	uint32 EntityPoolRefreshCount;

	/** Time between heartbeat events sent from clients to notify the servers they are still connected. */
	UPROPERTY(EditAnywhere, config, Category = "Heartbeat", meta = (ConfigRestartRequired = false, DisplayName = "Heartbeat Interval (seconds)"))
	float HeartbeatIntervalSeconds;

	/** Time that should pass since the last heartbeat event received to decide a client has disconnected. */
	UPROPERTY(EditAnywhere, config, Category = "Heartbeat", meta = (ConfigRestartRequired = false, DisplayName = "Heartbeat Timeout (seconds)"))
	float HeartbeatTimeoutSeconds;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (ConfigRestartRequired = false, DisplayName = "Construct Worker Boundaries"))
	bool ConstructWorkerBoundaries;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "World Dimension X"))
	int WorldDimensionX;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "World Dimension Z"))
	int WorldDimensionZ;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "Chunk Edge Length"))
	int ChunkEdgeLength;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "Cubes To Spawn Per Batch"))
	int CubesToSpawnAtATime;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "Delay Between Spawning Batch Of Cubes"))
	float DelayToSpawnNextGroup;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "Delay To Start Spawning Cubes"))
	float DelayToStartSpawningCubes;

	UPROPERTY(EditAnywhere, config, Category = "Visualizing Worker Boundaries", meta = (EditCondition = "ConstructWorkerBoundaries", ConfigRestartRequired = false, DisplayName = "Boundary Wall Scale Z"))
	float WallScaleZ;

	/**
	 * Limit the number of actors which are replicated per tick to the number specified.
	 * This acts as a hard limit to the number of actors per frame but nothing else. It's recommended to set this value to around 100~ (experimentation recommended).
	 * If set to 0, SpatialOS will replicate every actor per frame (unbounded) and so large worlds will experience slowdown server-side and client-side.
	 * Use `stat SpatialNet` in editor builds to find the number of calls to 'ReplicateActor' and use this to inform the rate limit setting.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Replication", meta = (ConfigRestartRequired = false, DisplayName = "Actor Replication Rate Limit"))
	uint32 ActorReplicationRateLimit;

	virtual FString ToString();
};

