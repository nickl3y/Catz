// © 2021, Brock Marsh. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DismembermentComponent.generated.h"

UENUM(BlueprintType)
enum class EDismemberColorChannel : uint8
{
	R_Channel UMETA(DisplayName="R"),
	G_Channel UMETA(DisplayName="G"),
	B_Channel UMETA(DisplayName="B"),
	A_Channel UMETA(DisplayName="A")
};

/*
 *	This handles caching the data from a dismemberment event. This
 *	is because animations wont update till the next frame so for the
 *	limb to copy the parents pose we have to delay a single frame.
 */
USTRUCT()
struct FDismemberedLimbFrameDelay
{
	GENERATED_BODY()

	FDismemberedLimbFrameDelay(){};
	FDismemberedLimbFrameDelay(const FName InName, USkeletalMeshComponent* InMesh)
	: BoneName(InName), SkeletalMeshComponent(InMesh){}
	
	UPROPERTY()
	FName BoneName = NAME_None;
	UPROPERTY()
	USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
	UPROPERTY()
	FVector Impulse = FVector(0);
};

/*
 *	Dismemberment Component is used for cutting off the
 *	limbs of a Skeletal Mesh. This component uses a control
 *	rig to scale bones based on their bone names.
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPreDismemberment, FName, BoneName, FVector, Impulse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPostDismemberment, FName, BoneName, USkeletalMeshComponent*, DismemberedMesh);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent, DisplayName = "Dismemberment Component"))
class DISMEMBERMENTSYSTEM_API UDismembermentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDismembermentComponent();

protected:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

/*
 *	Skeletal Mesh Component
 */
private:
	UPROPERTY()
	class USkeletalMeshComponent* SkeletalMeshComponent;
public:
	USkeletalMesh* GetMesh() const;
	UFUNCTION(BlueprintCallable, Category="Dismemberment")
	void SetSkeletalMeshComponentToDismember(class USkeletalMeshComponent* InSkeletalMeshComponent) {SkeletalMeshComponent = InSkeletalMeshComponent;};

	class USkeletalMeshComponent* GetSkeletalMeshComponent() const;

/*
*	Override Skeletal Mesh Components
*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Source Mesh", meta = (ToolTip="Optional setting to manually set the skeletal mesh(s) that should be used for dismemberment by the system. If false, the system will use the first Skeletal Mesh Component found on the Owning Actor"))
	bool bOverrideSourceMesh = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Source Mesh", meta = (EditCondition="bOverrideSkeletalMesh", EditConditionHides="true", GetOptions="GetSkeletalMeshOptions", ToolTip="Use this to manually set the skeletal mesh(s) that should be used for dismemberment by the system."))
	TArray<FString> SourceMeshComponents;
	UFUNCTION()
	TArray<FString> GetSkeletalMeshOptions() const;
	
	UPROPERTY()
	TArray<class USkeletalMeshComponent*> DismembermentMeshComponents;

	/* All Meshes attached to the Main Mesh. */
	TArray<class USkeletalMeshComponent*> GetAllAttachedMeshes() const;
	TArray<class USkeletalMeshComponent*> GetAllAttachedMeshes(USkeletalMeshComponent* Component) const;
	TArray<class USkeletalMeshComponent*> GetAllAttachedMeshes(FName LimbName) const;

	/* Gets All Meshes that Make up the Character (Mesh + GetAllAttachedMeshes())*/
	TArray<class USkeletalMeshComponent*> GetAllMeshes() const;

/*
*	Config
*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Dismemberment")
	bool bDisableDismemberment = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Dismemberment", meta=(EditCondition="bOverrideSkeletalMesh == false", EditConditionHides))
	bool bSupportAttachedChildMeshes = false;
	
	virtual void InitVertexColors();

	/** Physics scene information for this component, holds a single rigid body with multiple shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb Collision", meta=(ShowOnlyInnerProperties, SkipUCSModifiedProperties, ToolTip="The Collision settings that are used by the Limbs when dismembered."))
	FBodyInstance LimbCollisionSettings;

/*
*	Limb Data
*/
	UPROPERTY(BlueprintReadOnly, Category="LimbMap")
	class ULimbMap* LimbMap;
	
	bool IsLimbMapOutdated() const;
	void UpdateLimbMap();
	
	ULimbMap* CreateLimbMap();

	/*	A Map of every missing bone and the component that was spawned for it.
	 *	Every Child bone of a limb will be add to this map so that we dont chop
	 *	off limbs that do not exist. */
	UPROPERTY()
	TMap<FName, USkeletalMeshComponent*> MissingLimbs;
	void UpdateMissingLimbs(USkeletalMeshComponent* InDismemberedLimb, FName BoneName);

/*
 *	Replication
 */
	/* Used to replicate the Bones being dismembered for clients that didnt experience the dismember event. */
	UPROPERTY(ReplicatedUsing="OnRep_DismemberedBones")
	TArray<FName> DismemberedBones;
	TArray<FName> NetDeltaDismemberedBones;
	UFUNCTION()
	virtual void OnRep_DismemberedBones();

	/* Delay the OnRep_DismemberedBones so that we can allow the function to handle the dismemberment. */
	UFUNCTION()
	void DismemberPingDelayed();

	TArray<FName> GetNetDeltaDismemberedBones();
	
/*
*	Defaults
*/
	UPROPERTY(EditAnywhere, Category="Dismemberment", meta = (GetOptions="GetBoneNameOptions", ToolTip="These are bone names that will never get dismembered. Useful for Root or Pelvis bones."))
	TArray<FName> ExcludedBones;
	UPROPERTY(EditAnywhere, Category="Dismemberment", meta = (ToolTip="Opposite of Excluded Bones. If this is used then only bones contained in this list will be used."))
	TArray<FName> WhitelistBones;
	UFUNCTION()
	TArray<FString> GetBoneNameOptions() const;
	/* returns true if the bone name passes the bone filter. */
	bool CheckBoneFilter(FName BoneName) const;
	
	UPROPERTY(EditAnywhere, Category="Dismemberment", meta = (ToolTip="These are bone names that when dismembered, They will use the mapped name instead. Useful for pelvis bones."))
	TMap<FName, FName> RemappedBones;
	
	UPROPERTY(EditAnywhere, Category="Dismemberment", meta = (ToolTip="Set this to your Dismemberment Anim Instance. If Null, this will be set to the Ue4 Manniquin version that comes with the plugin"))
	TSubclassOf<class UDismemberedAnimInstance> DismemberedAnimInstance;

	int32 GetVertexNum(const int32 LOD = 0) const;
	int32 GetVertexNum(const USkeletalMeshComponent* InMesh, const int32 LOD = 0) const;

/*
*	Dismemberment
*/
	UPROPERTY()
	TArray<FDismemberedLimbFrameDelay> FrameDelayedDismemberedLimbs;

	UFUNCTION(BlueprintCallable, Category="Dismemberment", meta = (ToolTip="The Main function to call when Dismembering a Limb"))
	virtual void DismemberLimb(FName BoneName, FVector Impulse  = FVector(0));
	UFUNCTION(Server, WithValidation, Unreliable)
	void DismemberLimb_Server(FName BoneName, const FVector& Impulse);
	UFUNCTION(NetMulticast, WithValidation, Unreliable)
	void DismemberLimb_Multi(FName BoneName, const FVector& Impulse);
	UFUNCTION(BlueprintCallable, Category="Dismemberment", meta = (ToolTip="This will hide a limb on the Owning Mesh without spawning a dismembered limb with it."))
	virtual void DestroyLimb(const FName BoneName);

	/* This is the actual code that runs when a bone gets dismembered */
	UFUNCTION()
	virtual void DismemberLimb_Internal(FName BoneName, const FVector& Impulse);

	UFUNCTION()
	void BeginFrameDelayedDismemberment(USkeletalMeshComponent* Component, FName BoneName, FVector Impulse);
	UFUNCTION()
	void DismemberLimbFrameDelayed();

	USkeletalMeshComponent* CreateDismemberedLimb(const FName BoneName);

	void SetDismemberedAnimInstance(USkeletalMeshComponent* Component, FName BoneName) const;

	void RemovePreviouslyDismemberedLimbs(USkeletalMeshComponent* Component);

	void CreateDismemberPhysicsAsset(USkeletalMeshComponent* Component, FName InLimb) const;
	void TerminatePhysicsBodies(UPhysicsAsset* PhysicsAsset, int32 Index) const;

	static FBodyInstance* GetBodyInstance(USkeletalMeshComponent* Component, FName BoneName);

/*
*	Delegates
*/
	UPROPERTY(BlueprintAssignable)
	FOnPreDismemberment OnPreDismemberment;
	virtual void PreDismemberment(const FName BoneName, FVector Impulse);
	UPROPERTY(BlueprintAssignable)
	FOnPostDismemberment OnPostDismemberment;
	virtual void PostDismemberment(const FName BoneName, USkeletalMeshComponent* DismemberMesh);

/*
*	Vertex Based Damage Masks
*/
protected:
	void GetDismemberAlphas(const FName BoneName, TArray<FLinearColor>& Colors, TArray<FLinearColor>& Inverted);
	
	void ApplyMaskedVertexColors(const TArray<FLinearColor>& VertexColors) const;

/*
*	Limbs
*/
		/* Used to maintain dismembered limbs of child bones when dismembering a limbs parent | Ex.) Dismember Lower Arm then Dismember Upper Arm. */
	UPROPERTY()
	TArray<FLinearColor> DismemberedVertices;
		/* Combines the the two arrays using the greater number. NOTE: Make sure the Arrays are the same size. */
	TArray<float> CombineDismemberVertices(const TArray<float>& V1, const TArray<float>& V2);
		/* Combines the the two arrays using the greater number. NOTE: Make sure the Arrays are the same size. */
	TArray<FLinearColor> CombineDismemberVertices(const TArray<FLinearColor>& V1, const TArray<FLinearColor>& V2, EDismemberColorChannel Channel = EDismemberColorChannel::A_Channel);

	FLinearColor GetCurrentVertexColor(const USkeletalMeshComponent* Mesh, const int32 VertexIndex, const int32 LOD = 0) const;
	
	TArray<FLinearColor> GetCurrentVertexColors(const USkeletalMeshComponent* Mesh);
	TArray<FLinearColor> GetCurrentVertexColors(const int32 LODIndex = 0);
	TArray<FLinearColor> GetCurrentVertexColors(const USkeletalMeshComponent* Mesh, const int32 LODIndex = 0) const;
	
	void CopyVertexColorsToMesh(const USkeletalMeshComponent* FromMesh, USkeletalMeshComponent* ToMesh);

/*
*	Vertex Colors
*/
	TArray<FLinearColor> FColorArrayToLinear(const TArray<FColor>& Colors) const;
	
	static float GetColorOfChannel(const FLinearColor& Color, EDismemberColorChannel Channel);
	static void SetColorOfChannel(FLinearColor& Color, float Value, EDismemberColorChannel Channel);
	static void AddColorOfChannel(FLinearColor& Color, float Value, EDismemberColorChannel Channel);
	static void MaxColorOfChannel(FLinearColor& Color, float Value, EDismemberColorChannel Channel);
	static void MinColorOfChannel(FLinearColor& Color, float Value, EDismemberColorChannel Channel);
	
	static void SetLinearColorChannel(TArray<FLinearColor>& Colors, TArray<float>& Values, EDismemberColorChannel Channel);
	static void SetLinearColorChannel(TArray<FLinearColor>& Colors, float Value, EDismemberColorChannel Channel);
	static void AddLinearColorChannel(TArray<FLinearColor>& Colors, TArray<float>& Values, EDismemberColorChannel Channel);
	static void MaxLinearColorChannel(TArray<FLinearColor>& Colors, TArray<float>& Values, EDismemberColorChannel Channel);
	static void MinLinearColorChannel(TArray<FLinearColor>& Colors, TArray<float>& Values, EDismemberColorChannel Channel);

	static void InvertArrayValues(TArray<float>& Values);
	static void InvertLinearColorAlpha(TArray<FLinearColor>& Colors);

/*
*	Library
*/
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category="Dismemberment")
	static void SetAnimInstanceTargetSkeleton(UObject* InAnimInstance, USkeleton* InSkeleton);
#endif

/*
 *	Editor
 */
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
