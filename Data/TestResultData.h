// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TestResultData.generated.h"

// 단일 이미지의 메타데이터 정보
USTRUCT(BlueprintType)
struct FResultImageInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "ResultImageInfo")
    FName Category;

    UPROPERTY(BlueprintReadWrite, Category = "ResultImageInfo")
    FName TestName; // 식별이 쉽도록 ImageName에서 TestName으로 변경 (예: ChestCT)

    UPROPERTY(BlueprintReadWrite, Category = "ResultImageInfo")
    FName DetailItem;

    // 카테고리_검사명_인덱스 형태의 고유 키값을 반환합니다.
    FString GetUniqueKey() const
    {
        return Category.ToString() + TEXT("_") + TestName.ToString() + TEXT("_") + DetailItem.ToString();
    }
};

// 원본 바이트 배열을 포함하는 이미지 데이터 구조체
USTRUCT(BlueprintType)
struct FResultImage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "ResultImage")
    FName Category;

    UPROPERTY(BlueprintReadWrite, Category = "ResultImage")
    FName TestName;

    UPROPERTY(BlueprintReadOnly, Category = "ResultImage")
    FName DetailItem;

    UPROPERTY(BlueprintReadWrite, Category = "Scenario")
    TArray<uint8> Bytes;

    FString GetUniqueKey() const
    {
        return Category.ToString() + TEXT("_") + TestName.ToString() + TEXT("_") + DetailItem.ToString();
    }
};

USTRUCT(BlueprintType)
struct FByteDataBuffer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<uint8> Bytes;
};

// 혈액검사 표의 단일 행 데이터를 표기하기 위한 구조체입니다.
USTRUCT(BlueprintType)
struct FBloodTestRow
{
    GENERATED_BODY()

    // PH, pCO2, pO2 등의 세부 검사 항목명입니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    FName ParamName;

    // 수치 데이터 외에 측정 불가 표시(-) 등의 문자열 표현을 수용하기 위해 FString으로 처리합니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    FString ResultValue;

    // mmHg, mmol/L, % 등의 단위 문자열입니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    FString Unit;

    // 수치 비교를 통해 자동으로 결정되지만, UI 텍스트 바인딩 편의를 위해 변수 자체는 유지합니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    FString NormalStatus;

    // Panic 수치 해당 여부 혹은 관련 마킹 문자열입니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    FString PanicStatus;

    // 기존 ReferenceRange 문자열 대신 하한치와 상한치를 수치 데이터로 분리합니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    float ReferenceMin = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    float ReferenceMax = 0.0f;

    void UpdateNormalStatus()
    {
        if (ResultValue.IsEmpty() || ResultValue.Equals(TEXT("-")) || !ResultValue.IsNumeric())
        {
            NormalStatus = TEXT("");
            return;
        }

        float NumericValue = FCString::Atof(*ResultValue);

        if (NumericValue < ReferenceMin)
        {
            NormalStatus = TEXT("L");
        }
        else if (NumericValue > ReferenceMax)
        {
            NormalStatus = TEXT("H");
        }
        else
        {
            NormalStatus = TEXT("");
        }
    }

    // UFUNCTION을 제거하여 plain C++ 내부 전용 함수로 변경합니다.
    FString GetReferenceRangeString() const
    {
        return FString::Printf(TEXT("%.1f - %.1f"), ReferenceMin, ReferenceMax);
    }
};

// 여러 개의 행을 하나의 큰 검사 단위(예: 동맥혈가스분석, 일반혈액검사 등)로 묶어주는 컨테이너 구조체입니다.
USTRUCT(BlueprintType)
struct FBloodTestCategory
{
    GENERATED_BODY()

    // 큰 검사 분류 명칭입니다 (예: 동맥혈가스분석, 일반혈액검사 등).
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    FName Category;

    // 이 검사 그룹 하위에 소속된 세부 항목 표 데이터 배열입니다.
    UPROPERTY(BlueprintReadWrite, Category = "BloodTest")
    TArray<FBloodTestRow> Rows;
};