// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DiskSampler.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Category = "Disk Sampler")
class PROCTERRAINGEN_API UDiskSampler : public UObject
{
	GENERATED_BODY()

	int mRows;
	int mColumns;
	float mWidth;
	float mHeight;
	int mRadius;
	float mCellSize;

	FIntPoint GetGridCellFromPosition(const FVector2D& Position);
	bool IsPointValid(const FVector2D& Point);

	int* DataGrid;
	TArray<FVector2D> ActiveList;
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Points")
	TArray<FVector2D> PointList;

	UFUNCTION(BlueprintCallable, Category="Disk Sampler")
	void GeneratePoints(int64 seed, float width, float height, float radius, int k);

};
