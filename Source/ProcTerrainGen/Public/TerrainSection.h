// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SimplexNoiseBPLibrary.h"
#include "TerrainSection.generated.h"
#include "HAL/Runnable.h"

class FRunnableThread;

FVector2D CalculateWorldCoordinatesFromTerrainCoords(const FIntPoint& TerrainCoords, const FVector2D& SectionSize);

class UDiskSampler;
class ALandscapeGenerator;

/**
 * 
 */
UCLASS()
class PROCTERRAINGEN_API UTerrainSection : public UObject
{
	GENERATED_BODY()

	TArray<FVector> mSectionVertices;
	TArray<int32> mSectionIndices;
	TArray<FVector> mSectionNormals;
	FVector2D mSectionSize;
	ALandscapeGenerator* mLandscapeGen;

	TArray<FVector> mSectionLODVertices;
	TArray<int32> mSectionLODIndices;
	TArray<FVector> mSectionLODNormals;

	//Landscape Data
	FVector mMeshOrigin;
	FIntPoint mComponentsPerAxis;
	float mNoiseScale;
	float mHeightScale;
	float mLacunarity;
	float mPersistance;
	int mOctaves;

	bool IsOriginCoord(const FVector& PlayerLocation);

public:
	void InitialiseSection(ALandscapeGenerator* LandscapeGen, int SectionGridIndex, FIntPoint TerrainCoords, uint32 Seed, const FVector2D& SectionSize, const FIntPoint& ComponentsPerAxis, float fNoiseScale, float fHeightScale, float fLacunarity, float fPersistance, int Octaves);
	void GenerateSectionMeshData();
	bool GenerateLODData(int LOD);
	void UseLODLevel(int LOD);
	FVector CalculateVertexPosition(float xPos, float yPos);
	void UpdateTerrainSection();
	void RemoveSection();

	FIntPoint mTerrainCoords;
	bool bMeshGenerated;
	int mSectionIndex;
	int LODLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Points")
	UDiskSampler* Points;
};


class FGeneratorThread : public FRunnable
{
public:
	FGeneratorThread();

	virtual ~FGeneratorThread() override;

	bool Init() override;
	uint32 Run() override;
	void Stop() override;

private:
	FRunnableThread* Thread;

	bool bRunThread;
};