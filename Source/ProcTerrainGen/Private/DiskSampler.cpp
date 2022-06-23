// Fill out your copyright notice in the Description page of Project Settings.


#include "DiskSampler.h"


void UDiskSampler::GeneratePoints(int64 seed, float width, float height, float radius, int k)
{
	FMath::RandInit(seed);
	PointList.Empty();
	ActiveList.Empty();

	mWidth = width;
	mHeight = height;
	mRadius = radius;
	// radius / sqrt(2)
	mCellSize = radius / 1.41421356f;

	mRows = FMath::CeilToInt(width / mCellSize);
	mColumns = FMath::CeilToInt(height / mCellSize);

	int numCount = mRows * mColumns;

	DataGrid = (int *) FMemory::Malloc(sizeof(int) * numCount);
	for (int a = 0; a < numCount; a++)
		DataGrid[a] = -1;

	//Generate Initial Point
	FVector2D InitialPos(FMath::FRand() * width, FMath::FRand() * height);

	//calculate index of point in the grid
	FIntPoint GridCell = GetGridCellFromPosition(InitialPos);
	
	int ListIndex = PointList.Add(InitialPos);
	DataGrid[GridCell.X + GridCell.Y * mColumns] = ListIndex;
	ActiveList.Add(InitialPos);

	while (!ActiveList.IsEmpty())
	{
		int randIndex = FMath::RandRange(0, ActiveList.Num() - 1);
		FVector2D origin = ActiveList[randIndex];
		bool found = false;
		for (int i = 0; i < k; i++)
		{
			//Search in range r^2 to (2r)^2 to obtain a uniform distribution then sqrt result
			float VScale = FMath::Sqrt(FMath::RandRange(FMath::Pow(mRadius, 2), FMath::Pow(2 * mRadius, 2)));
			FVector2D NewPoint = origin + FVector2D(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal() * VScale;

			if (IsPointValid(NewPoint))
			{
				found = true;

				FIntPoint PointCell = GetGridCellFromPosition(NewPoint);
				int NewPointIndex = PointList.Add(NewPoint);
				ActiveList.Add(NewPoint);
				DataGrid[PointCell.X + PointCell.Y * mColumns] = NewPointIndex;
				break;
			}
		}

		if (!found)
			ActiveList.RemoveAt(randIndex);
	}

	FMemory::Free(DataGrid);
}

FIntPoint UDiskSampler::GetGridCellFromPosition(const FVector2D& Position)
{
	FIntPoint GridCell;
	GridCell.X = (int) (Position.X / mCellSize);
	GridCell.Y = (int) (Position.Y / mCellSize);

	return GridCell;
}

bool UDiskSampler::IsPointValid(const FVector2D& Point)
{
	//Test for point validity
	if (Point.X > mWidth || Point.X < 0 || Point.Y < 0 || Point.Y > mHeight)
		return false;

	FIntPoint GridCell = GetGridCellFromPosition(Point);

	int StartX = FMath::Max(0, GridCell.X - 2);
	int StartY = FMath::Max(0, GridCell.Y - 2);

	int EndX = FMath::Min(mColumns - 1, GridCell.X + 2);
	int EndY = FMath::Min(mRows - 1, GridCell.Y + 2);

	for (int i = StartX; i <= EndX; i++)
	{
		for (int j = StartY; j <= EndY; j++)
		{
			int PointIndex = DataGrid[i + j * mColumns];
			if (PointIndex != -1)
			{
				FVector2D CorrespondingPoint = PointList[PointIndex];
				float distance = FVector2D::DistSquared(CorrespondingPoint, Point);

				if (distance < (mRadius*mRadius))
					return false;
			}
		}
	}

	return true;
}

