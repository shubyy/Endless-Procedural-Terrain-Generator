// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SimplexNoiseBPLibrary.h"
#include "HAL/Runnable.h"
#include "TerrainSection.generated.h"


class FRunnableThread;

FVector2D CalculateWorldCoordinatesFromTerrainCoords(const FIntPoint& TerrainCoords, const FVector2D& SectionSize);

class UDiskSampler;
class ALandscapeGenerator;
class FGeneratorThread;

/**
 * 
 */
UCLASS()
class PROCTERRAINGEN_API UTerrainSection : public UObject
{
	GENERATED_BODY()
	
public:
	void InitialiseSection(ALandscapeGenerator* LandscapeGen, int SectionGridIndex, FIntPoint TerrainCoords, uint32 Seed, const FVector2D& SectionSize, const FIntPoint& ComponentsPerAxis, float fNoiseScale, float fHeightScale, float fLacunarity, float fPersistance, int Octaves);
	bool IsOriginCoord(const FVector& PlayerLocation);
	bool GenerateLODData(int LOD);
	FVector CalculateVertexPosition(float xPos, float yPos);
	void GenerateSectionMeshData();
	void UpdateTerrainSection(int LOD);
	void RemoveSection();

	FGeneratorThread *Thread;

	FIntPoint mTerrainCoords;
	bool bMeshGenerated;
	int mSectionIndex;
	int LODLevel;
	int GenLOD;
	bool GeneratingLOD;

	//Landscape Data
	FVector mMeshOrigin;
	FIntPoint mComponentsPerAxis;
	float mNoiseScale;
	float mHeightScale;
	float mLacunarity;
	float mPersistance;
	int mOctaves;

	TArray<FVector> mSectionVertices;
	TArray<int32> mSectionIndices;
	TArray<FVector> mSectionNormals;
	FVector2D mSectionSize;
	ALandscapeGenerator* mLandscapeGen;

	TArray<FVector> mSectionLODVertices;
	TArray<int32> mSectionLODIndices;
	TArray<FVector> mSectionLODNormals;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Points")
	UDiskSampler* Points;
};

enum THREAD_OPERATION {
	GEN_LANDSCAPE,
	GEN_LOD,
	GEN_POINTDISC,
};


class FGeneratorThread : public FRunnable
{
public:
	FGeneratorThread(UTerrainSection* Section);
	bool StartOperation(THREAD_OPERATION operation);

	virtual ~FGeneratorThread() override;

	bool Init() override;
	uint32 Run() override;
	void Stop() override;
	
private:
	FRunnableThread* Thread;
	UTerrainSection* mSection;
	THREAD_OPERATION mOperation;


	bool bRunThread;
};