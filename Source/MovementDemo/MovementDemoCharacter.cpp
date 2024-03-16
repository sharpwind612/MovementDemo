// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovementDemoCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AMovementDemoCharacter

AMovementDemoCharacter::AMovementDemoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMovementDemoCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMovementDemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMovementDemoCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMovementDemoCharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMovementDemoCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMovementDemoCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

bool AMovementDemoCharacter::ForwardTracer(FHitResult& HitResult) const
{
	const FVector Location = GetActorLocation();
	const FVector ForwardVector = GetActorForwardVector();
	const FVector StartPose = Location;
	const FVector EndPose = Location + ForwardVector * 100;
	const TArray<AActor*> Actors;
	if (UKismetSystemLibrary::LineTraceSingle(this, StartPose, EndPose, ETraceTypeQuery::TraceTypeQuery1, false, Actors, EDrawDebugTrace::ForDuration, HitResult, true))
	{
		return true;
	}
	return false;
}

bool AMovementDemoCharacter::DownwardTracer(const FVector& Location, FHitResult& HitResult) const
{
	// const FVector StartP = GetActorLocation();
	// const FVector ForwardVector = GetActorForwardVector();
	const FVector StartPose = Location + FVector(0,0,200.f);
	const FVector EndPose = StartPose + FVector(0,0,-300.f);
	const TArray<AActor*> Actors;
	if (UKismetSystemLibrary::LineTraceSingle(this, StartPose, EndPose, ETraceTypeQuery::TraceTypeQuery1, false, Actors, EDrawDebugTrace::ForDuration, HitResult, true))
	{
		return true;
	}
	return false;
}

void AMovementDemoCharacter::DoClimb(const FVector& HitLocation,const FVector& CustomOffset, const FVector& WallNormal, UAnimMontage* Montage)
{
	//When character is climbing, forbid to do it twice
	if(bIsClimbing)
	{
		return;
	}
	bIsClimbing = true;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->SetMovementMode(EMovementMode::MOVE_Flying);
	//Calculate Actor Location
	const FVector NewLocation = HitLocation + FVector(CustomOffset.X,0,CustomOffset.Z) + WallNormal * CustomOffset.Y;
	SetActorLocation(NewLocation);
	//Calculate Actor Rotation
	const FRotator NewRotation = UKismetMathLibrary::MakeRotationFromAxes(-WallNormal, GetActorRightVector(), FVector::Zero());
	SetActorRotation(NewRotation);
	//Play Montage
	if (USkeletalMeshComponent* TppMesh = GetMesh())
	{
		if (UAnimInstance* AnimInstance = TppMesh->GetAnimInstance())
		{
			PlayAnimMontage(Montage);
			if (!MontageEndedDelegate.IsBoundToObject(this))
			{
				MontageEndedDelegate.BindUObject(this, &AMovementDemoCharacter::OnClimbMontageEnded);
			}
			AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, Montage);
		}
	}
}

void AMovementDemoCharacter::OnClimbMontageEnded_Implementation(UAnimMontage* Montage, bool bInterrupted)
{
	bIsClimbing = false;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->SetMovementMode(EMovementMode::MOVE_Walking);
}

void AMovementDemoCharacter::Jump()
{
	bool bShouldJump = true;

	FHitResult HitResult;
	if (ForwardTracer(HitResult))
	{
		GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Blue, HitResult.GetActor()->GetName());
		FVector HitLocation = HitResult.Location;
		FVector WallNormal = HitResult.Normal;
		FHitResult HitDownwardResult;
		if (DownwardTracer(HitLocation - WallNormal, HitDownwardResult))
		{
			FVector HitDownwardLocation = HitDownwardResult.Location;
			float HitDownwardDistance = HitDownwardResult.Distance;
			GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Cyan, FString::Printf(TEXT("Distance:%f"), HitDownwardDistance));
			if (HitDownwardDistance > 0 && HitDownwardDistance < 250.f)
			{
				bShouldJump = false;
				//High Wall
				if (HitDownwardDistance < 100.f)
				{
					FSoftObjectPath AnimPath = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Animations/Manny/MQ_Climb_RM_UE5_Montage.MQ_Climb_RM_UE5_Montage")) ;
					UObject* MontageObject = AnimPath.ResolveObject();
					if (!MontageObject)
					{
						MontageObject = AnimPath.TryLoad();
					}
					UAnimMontage* Montage = Cast<UAnimMontage>(MontageObject);
					if (Montage)
					{
						DoClimb(HitDownwardLocation, FVector(0,40,-70), WallNormal, Montage);
						GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Cyan, FString::Printf(TEXT("StartClimbHigh:%s"), *Montage->GetName()));
					}
					else
					{
						GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, "StartClimbHigh:Anim Not Found!!!");
					}
				}
				else
				{
					FSoftObjectPath AnimPath = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Animations/Manny/MQ_GettingUp_RM_UE5_Montage.MQ_GettingUp_RM_UE5_Montage")) ;
					UObject* MontageObject = AnimPath.ResolveObject();
					if (!MontageObject)
					{
						MontageObject = AnimPath.TryLoad();
					}
					UAnimMontage* Montage = Cast<UAnimMontage>(MontageObject);
					if (Montage)
					{
						DoClimb(HitDownwardLocation, FVector(0,90,-20), WallNormal, Montage);
						GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Cyan, FString::Printf(TEXT("StartClimbLow:%s"), *Montage->GetName()));
					}
					else
					{
						GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, "StartClimbLow:Anim Not Found!!!");
					}
				}
			}
		}
	}
	if (bShouldJump)
	{
		Super::Jump();
	}
}

void AMovementDemoCharacter::StopJumping()
{
	Super::StopJumping();
}

void AMovementDemoCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AMovementDemoCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}