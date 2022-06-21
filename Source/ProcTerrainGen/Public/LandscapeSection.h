// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandscapeSection.generated.h"

class ALandscapeGenerator;
class UDiskSampler;
class FGeneratorThread;
class FRunnableThread;
class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class PROCTERRAINGEN_API ALandscapeSection : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALandscapeSection();
	void InitialiseSection(ALandscapeGenerator* LandscapeGen, FIntPoint TerrainCoords, uint32 Seed, const FVector2D& SectionSize, const FIntPoint& ComponentsPerAxis, float fNoiseScale, float fHeightScale, float fLacunarity, float fPersistance, int Octaves);
	bool IsOriginCoord(const FVector& PlayerLocation);
	bool GenerateLODData(int LOD);
	FVector CalculateVertexPosition(float xPos, float yPos);
	void GenerateFoliage();
	void RemoveFoliage();
	void GenerateSectionMeshData();
	void UpdateTerrainSection(int LOD);
	void RemoveSection();

	bool FoliageGenerated;
	bool PointsGenerated;
	bool bMeshGenerated;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Mesh")
	UProceduralMeshComponent* mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape Mesh")
	UHierarchicalInstancedStaticMeshComponent* InstMesh;

	//Multithreader
	FGeneratorThread* Thread;

	//Section Info
	FIntPoint mTerrainCoords;
	int LODLevel;
	int GenLOD;
	bool GeneratingLOD;

	//Section Data
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points")
	UDiskSampler* Points;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	

};

enum THREAD_OPERATION {
	GEN_LANDSCAPE,
	GEN_LOD,
	GEN_POINTDISC,
};

class FGeneratorThread : public FRunnable
{
public:
	FGeneratorThread(ALandscapeSection* Section);
	bool StartOperation(THREAD_OPERATION operation);

	virtual ~FGeneratorThread() override;

	bool Init() override;
	uint32 Run() override;
	void Stop() override;

private:
	FRunnableThread* Thread;
	ALandscapeSection* mSection;
	THREAD_OPERATION mOperation;

	bool bRunThread;
};