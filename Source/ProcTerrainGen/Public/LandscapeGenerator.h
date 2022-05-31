// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "TerrainSection.h"
#include "LandscapeGenerator.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGeneratedDelegate);

UCLASS()
class PROCTERRAINGEN_API ALandscapeGenerator : public AActor
{
	GENERATED_BODY()

	FIntPoint CalcVisibleGridPoints(const FVector& PlayerLocation);
	void GenerateNewTerrainGrid();
	UTerrainSection* DoesTerrainCoordExist(const FIntPoint& TerrainCoord);
	bool IsTerrainCoordVisible(const FIntPoint& Coord);
	int FindAndReserveFreeSectionIndex();
	bool IsCloseForCollision(const FIntPoint& Coords, const FVector& PlayerLocation);
	void FreeSectionIndex(int index);
	int CalcLODLevelFromTerrainCoordDistance(float Distance);

	TArray<FVector> LandscapeVertices;
	TArray<int32> LandscapeIndices;
	TArray<FVector> LandscapeNormals;
	TArray<bool> ReservedSections;
	TArray<FIntPoint> VisibleGridCoords;
	
	int NoiseSeed;
	bool mGenerating;
	bool bCanGenerate;

public:	
	// Sets default values for this actor's properties
	ALandscapeGenerator();
	float ApplyTerrainHeightMultiplier(float value);
	FIntPoint GetCurrentGridPoint(const FVector& PlayerLocation);

	//Get landscape material
	UMaterialInterface* GetMaterial();

	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Properties")
	FVector2D LandscapeSectionSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Properties")
	FVector2D LandscapeUVScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Properties")
	FIntPoint LandscapeComponentSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Properties")
	UMaterialInterface *LandscapeMat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Properties")
	int MaxSectionCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Properties")
	int GenerationLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Noise")
	float fPersistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Noise")
	int Octaves;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Noise")
	float fLacunarity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Noise")
	float fNoiseScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Noise")
	float fHeightScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Noise")
	UCurveFloat *TerrainHeight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Data")
	TArray<UTerrainSection*> SectionObjects;

	UPROPERTY(BlueprintAssignable, Category = "Landscape Functions")
	FGeneratedDelegate OnGenerated;

	UFUNCTION(BlueprintCallable, Category = "Landscape Functions")
	void StartGeneration();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual void PostActorCreated() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
