#include "EstCore.h"
#include "EstBaseCharacter.h"
#include "UnrealNetwork.h"
#include "EstBaseWeapon.h"

AEstBaseWeapon::AEstBaseWeapon(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("WeaponMesh"));
	WeaponMesh->SetCollisionProfileName("Item");
	WeaponMesh->bCastInsetShadow = true;
	SetRootComponent(WeaponMesh);

	bReplicates = true;
}

void AEstBaseWeapon::OnPostRestore_Implementation()
{
	if (OwnerActorName == NAME_None)
	{
		return;
	}

	for (TActorIterator<AEstBaseCharacter> Character(GetWorld()); Character; ++Character)
	{
		if (Character->GetFName() == OwnerActorName)
		{
			Character->EquipWeapon(this);
			return;
		}
	}
}

void AEstBaseWeapon::OnPreSave_Implementation()
{
	OwnerActorName = GetOwner() ? OwnerActorName = GetOwner()->GetFName() : NAME_None;
}

void AEstBaseWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(AEstBaseWeapon, bIsPrimaryFirePressed);
	DOREPLIFETIME(AEstBaseWeapon, bIsSecondaryFirePressed);
	DOREPLIFETIME(AEstBaseWeapon, bIsHolstered);
}

void AEstBaseWeapon::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UpdateWeaponPhysicsState();
}

void AEstBaseWeapon::UpdateWeaponPhysicsState()
{
	if (IsEquipped())
	{
		WeaponMesh->SetSimulatePhysics(false);
		SetActorEnableCollision(false);
	}
	else
	{
		WeaponMesh->SetSimulatePhysics(true);
		SetActorEnableCollision(true);
	}
}

bool AEstBaseWeapon::OnUsed_Implementation(AEstBaseCharacter* User, class USceneComponent* UsedComponent)
{
	SetOwner(User);
	User->EquipWeapon(this);
	return true;
}

void AEstBaseWeapon::SetEngagedInActivity(float ActivityLength)
{
	ensure(!IsEngagedInActivity());
	LastActivityFinishTime = GetWorld()->TimeSeconds + ActivityLength;
}

void AEstBaseWeapon::PrimaryAttackStart_Implementation()
{
	UE_LOG(LogEstReplication, Warning, TEXT("PrimaryAttackStart_Implementation"));

	Unholster();
	bIsPrimaryFirePressed = true;
}

bool AEstBaseWeapon::PrimaryAttackStart_Validate()
{
	return true;
}

void AEstBaseWeapon::PrimaryAttackEnd_Implementation()
{
	UE_LOG(LogEstReplication, Warning, TEXT("PrimaryAttackEnd_Implementation"));

	bIsPrimaryFirePressed = false;
}

bool AEstBaseWeapon::PrimaryAttackEnd_Validate()
{
	return true;
}


void AEstBaseWeapon::SecondaryAttackStart_Implementation()
{
	Unholster();
	bIsSecondaryFirePressed = true;
}

bool AEstBaseWeapon::SecondaryAttackStart_Validate()
{
	return true;
}

void AEstBaseWeapon::SecondaryAttackEnd_Implementation()
{
	bIsSecondaryFirePressed = false;
}

bool AEstBaseWeapon::SecondaryAttackEnd_Validate()
{
	return true;
}

void AEstBaseWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsPrimaryFirePressed)
	{
		return PrimaryAttack_Implementation();
	}

	if (bIsSecondaryFirePressed)
	{
		return SecondaryAttack_Implementation();
	}
}

void AEstBaseWeapon::PrimaryAttack_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("PrimaryAttack_Implementation"));

	if (IsEngagedInActivity())
	{
		return;
	}

	if (WeaponAnimManifest.PrimaryAttack && WeaponMesh->GetAnimInstance())
	{
		WeaponMesh->GetAnimInstance()->Montage_Play(WeaponAnimManifest.PrimaryAttack);
		SetEngagedInActivity(PrimaryAttackLength);
	}

	OnPrimaryAttack.Broadcast();
}

bool AEstBaseWeapon::PrimaryAttack_Validate()
{
	return true;
}

void AEstBaseWeapon::SecondaryAttack_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("SecondaryAttack_Implementation"));

	if (IsEngagedInActivity())
	{
		return;
	}

	if (WeaponAnimManifest.SecondaryAttack && WeaponMesh->GetAnimInstance())
	{
		WeaponMesh->GetAnimInstance()->Montage_Play(WeaponAnimManifest.SecondaryAttack);
		SetEngagedInActivity(SecondaryAttackLength);
	}

	OnSecondaryAttack.Broadcast();
}

bool AEstBaseWeapon::SecondaryAttack_Validate()
{
	return true;
}

void AEstBaseWeapon::Equip(AEstBaseCharacter* OwnerCharacter)
{
	SetOwner(OwnerCharacter);

	UpdateWeaponPhysicsState();
	OnEquip();
	Unholster();

	OnEquipped.Broadcast();
}

void AEstBaseWeapon::Unequip()
{
	const AActor* PreviousOwner = GetOwner();
	SetOwner(nullptr);

	bIsPrimaryFirePressed = false;
	bIsSecondaryFirePressed = false;

	if (WeaponMesh->GetAnimInstance())
	{
		WeaponMesh->GetAnimInstance()->Montage_Stop(0.f);
	}

	UpdateWeaponPhysicsState();

	FVector EyeLoc;
	FRotator EyeRot;
	PreviousOwner->GetActorEyesViewPoint(EyeLoc, EyeRot);
	SetActorLocationAndRotation(EyeLoc, EyeRot);

	if (!PreviousOwner->ActorHasTag(TAG_DEAD))
	{
		WeaponMesh->AddImpulse(EyeRot.Vector() * 1000.f);
	}

	OnUnequip();
}

bool AEstBaseWeapon::IsEquipped()
{
	if (!GetOwner())
	{
		return false;
	}

	AEstBaseCharacter* Character = Cast<AEstBaseCharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	return Character->EquippedWeapon == this;
}

void AEstBaseWeapon::Holster()
{
	OnHolster();

	bIsHolstered = true;
}

void AEstBaseWeapon::Unholster()
{
	OnUnholster();

	bIsHolstered = false;
}