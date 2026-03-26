void ULineageAIPopulationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (const ULineageAISettings* Defaults = ULineageAISettings::Get())
    {
        CurrentSimulationSettings = Defaults->DefaultSettings;
    }
}

void ULineageAIPopulationSubsystem::Deinitialize()
{
    OnAsyncOperationFinished.Clear();
    JobQueue.Empty();
    bIsBusy = false;

    ClearPopulation();
    LivingCharacters.Shrink();
    DeceasedCharacters.Shrink();

    AllTraits.Empty();
    CachedTraitGroupRules.Empty();
    CachedMaleNames.Empty();
    CachedFemaleNames.Empty();
    CachedSurnames.Empty();

    if (GEngine) GEngine->ForceGarbageCollection(true);

    // Forces the OS to reclaim the fragmented memory pool after massive TMap destruction
    FMemory::Trim();

    Super::Deinitialize();
}

// =====================================================================
// ASYNC JOB QUEUE MANAGEMENT
// =====================================================================

void ULineageAIPopulationSubsystem::EnqueueJob(const FString& Name, TFunction<void()> Work)
{
    JobQueue.Add({ Name, MoveTemp(Work) });

    if (!bIsBusy)
    {
        ProcessNextJob();
    }
}

void ULineageAIPopulationSubsystem::ProcessNextJob()
{
    if (JobQueue.IsEmpty())
    {
        bIsBusy = false;
        return;
    }

    bIsBusy = true;
    FLineageAsyncJob CurrentJob = JobQueue[0];
    JobQueue.RemoveAt(0);

    CurrentJob.WorkPayload();
}

// =====================================================================
// DATA ACCESS & MANAGEMENT
// =====================================================================

bool ULineageAIPopulationSubsystem::AddCharacter(const FCharacterData& Character)
{
    FCharacterData Copy = Character;
    if (!Copy.ID.IsValid()) Copy.ID = FGuid::NewGuid();

    if (LivingCharacters.Contains(Copy.ID)) return false;

    LivingCharacters.Add(Copy.ID, MoveTemp(Copy));
    return true;
}

bool ULineageAIPopulationSubsystem::UpdateCharacter(const FCharacterData& UpdatedCharacterData, bool bWriteToDB)
{
    if (!UpdatedCharacterData.ID.IsValid()) return false;

    bool bUpdatedRAM = false;

    if (FCharacterData* FoundLiving = LivingCharacters.Find(UpdatedCharacterData.ID))
    {
        *FoundLiving = UpdatedCharacterData;
        bUpdatedRAM = true;
    }
    else if (FCharacterData* FoundDeceased = DeceasedCharacters.Find(UpdatedCharacterData.ID))
    {
        *FoundDeceased = UpdatedCharacterData;
        UnsavedDeceasedIDs.Add(UpdatedCharacterData.ID);
        bUpdatedRAM = true;
    }

    if (bWriteToDB)
    {
        if (ULineageAIDatabaseSubsystem* DBSubsystem = GetDatabaseSubsystem())
        {
            return DBSubsystem->AddOrUpdateCharacterInDatabase(UpdatedCharacterData);
        }
        return false;
    }

    return bUpdatedRAM;
}

bool ULineageAIPopulationSubsystem::GetCharacter(const FGuid& Id, FCharacterData& OutCharacter) const
{
    if (const FCharacterData* FoundLiving = LivingCharacters.Find(Id))
    {
        OutCharacter = *FoundLiving;
        return true;
    }

    if (const FCharacterData* FoundDeceased = DeceasedCharacters.Find(Id))
    {
        OutCharacter = *FoundDeceased;
        return true;
    }

    if (ULineageAIDatabaseSubsystem* DBSubsystem = GetDatabaseSubsystem())
    {
        FCharacterData FoundChar;
        if (DBSubsystem->GetCharacterFromDatabase(Id, FoundChar))
        {
            // Cache the lazy-loaded deceased character to prevent repeated DB hits
            DeceasedCharacters.Emplace(Id, FoundChar);
            OutCharacter = MoveTemp(FoundChar);
            return true;
        }
    }
    return false;
}

void ULineageAIPopulationSubsystem::ClearPopulation()
{
    LivingCharacters.Empty();
    DeceasedCharacters.Empty();
}

// =====================================================================
// CORE SIMULATION LOGIC (PROPRIETARY SECTIONS OMITTED)
// =====================================================================

bool ULineageAIPopulationSubsystem::AssignStartingTraits_ThreadSafe(FCharacterData& OutCharacter, const TMap<FName, FCharacterTrait>& TraitPoolSnapshot, const TMap<FName, FTraitGroupDefinition>& GroupPoolSnapshot, const TArray<FName>& TraitKeysSnapshot) const
{
    // PROPRIETARY LOGIC OMITTED FOR SHOWCASE
    // Handles complex evaluation of mandatory/exclusive trait groups and applies initial genetic 
    // attributes to characters securely within the worker thread.
    return true;
}

TArray<FName> ULineageAIPopulationSubsystem::CalculateInheritedTraits(const FCharacterData& ParentA, const FCharacterData& ParentB, const TMap<FName, FCharacterTrait>& TraitPool, const TMap<FName, FTraitGroupDefinition>& TraitGroupPool)
{
    // PROPRIETARY LOGIC OMITTED FOR SHOWCASE
    // Rolls Mendelian dominance probabilities based on the parents' genetics to determine 
    // the offspring's trait array, resolving mutual exclusions (e.g., eye color).
    return TArray<FName>();
}

void ULineageAIPopulationSubsystem::ProcessMortality(TMap<FGuid, FCharacterData>& ActiveLiving, int32 CurrentYear, const FLineageSimulationSettings& Settings, TSet<FGuid>& OutDeceasedIDs, TMap<FGuid, FString>& OutDeathDates) const
{
    // PROPRIETARY LOGIC OMITTED FOR SHOWCASE
    // Evaluates curve-based mortality rates, age decline algorithms, and accident chances 
    // to flag characters for death during the background simulation tick.
}

void ULineageAIPopulationSubsystem::ProcessMarriages(TMap<FGuid, FCharacterData>& ActiveLiving, int32 CurrentYear, const FLineageSimulationSettings& Settings, const TSet<FGuid>& DeceasedIDsThisYear, TArray<TTuple<FGuid, FGuid, FName>>& OutMarriages) const
{
    // PROPRIETARY LOGIC OMITTED FOR SHOWCASE
    // Matches eligible bachelors and bachelorettes based on age difference tolerances, 
    // updating surnames and relational links securely within the thread state.
}

void ULineageAIPopulationSubsystem::ProcessFertility(TMap<FGuid, FCharacterData>& ActiveLiving, int32 CurrentYear, const FLineageSimulationSettings& Settings, const TSet<FGuid>& DeceasedIDsThisYear, const TMap<FName, FCharacterTrait>& TraitPoolSnapshot, const TMap<FName, FTraitGroupDefinition>& GroupPoolSnapshot, const TArray<FName>& MaleNamesSnapshot, const TArray<FName>& FemaleNamesSnapshot, TArray<FCharacterData>& OutNewChildren)
{
    // PROPRIETARY LOGIC OMITTED FOR SHOWCASE
    // Calculates pregnancy chances against current population carrying capacity and creates 
    // entirely new FCharacterData instances, including their inherited traits.
}

// =====================================================================
// ASYNC SIMULATION EXECUTION
// =====================================================================

void ULineageAIPopulationSubsystem::StartNewSimulationAsync(const FLineageSimulationSettings& NewSettings, FString SaveName, int32 FounderFamilies, int32 SimulationYears)
{
    CurrentSimulationSettings = NewSettings;

    EnqueueJob("Starting New Game", [this, SaveName, FounderFamilies, SimulationYears]() mutable
        {
            if (!SaveName.EndsWith(TEXT(".db"))) SaveName += TEXT(".db");

            ClearPopulation();

            if (ULineageAIDatabaseSubsystem* DB = GetDatabaseSubsystem())
            {
                bool bSuccess = false;
                FString OutMessage;

                DB->OpenDatabase(bSuccess, OutMessage, SaveName);

                if (!bSuccess)
                {
                    UE_LOG(LogLineageAI, Error, TEXT("StartNewSimulation: Failed to open/create database '%s'. Error: %s"), *SaveName, *OutMessage);
                    ProcessNextJob();
                    return;
                }

                DB->ExecuteQuery(TEXT("BEGIN TRANSACTION;"));
                DB->ExecuteQuery(TEXT("DELETE FROM Characters;"));
                DB->ExecuteQuery(TEXT("COMMIT;"));

                DB->SaveSimulationSettings_Internal(CurrentSimulationSettings);

                WorldYear = 1; WorldMonth = 1; WorldDay = 1; WorldHour = 8;
                DB->SaveWorldTime(WorldYear, WorldMonth, WorldDay, WorldHour, 0);
            }
            else
            {
                ProcessNextJob();
                return;
            }

            // (Initialization logic omitted for showcase brevity)

            if (SimulationYears > 0)
            {
                AdvanceTimeAsync(SimulationYears);
            }
            else
            {
                OnAsyncOperationFinished.Broadcast(ELineageJobType::Generation, true, FString::Printf(TEXT("New World '%s' Created"), *SaveName));
            }

            ProcessNextJob();
        });
}

void ULineageAIPopulationSubsystem::AdvanceTimeAsync(int32 YearsToAdvance)
{
    if (YearsToAdvance <= 0) return;

    FLineageSimulationSettings ThreadSettings = CurrentSimulationSettings;
    TWeakObjectPtr<ULineageAIPopulationSubsystem> WeakSelf(this);

    // Create deep copies (snapshots) for thread safety
    TMap<FGuid, FCharacterData> LocalLiving = MoveTemp(LivingCharacters);
    TMap<FName, FCharacterTrait> TraitPoolSnapshot = AllTraits;
    TMap<FName, FTraitGroupDefinition> GroupPoolSnapshot = CachedTraitGroupRules;
    TArray<FName> TraitKeysSnapshot = CachedTraitKeys;
    TArray<FName> MaleNamesSnapshot = CachedMaleNames;
    TArray<FName> FemaleNamesSnapshot = CachedFemaleNames;
    int32 LocalYear = WorldYear;

    DeceasedCharacters.Empty();

    EnqueueJob(FString::Printf(TEXT("Simulating %d Years"), YearsToAdvance),
        [WeakSelf, YearsToAdvance, ThreadSettings, LocalLiving = MoveTemp(LocalLiving), LocalYear,
        TraitPoolSnapshot, GroupPoolSnapshot, TraitKeysSnapshot, MaleNamesSnapshot, FemaleNamesSnapshot]() mutable
        {
            Async(EAsyncExecution::Thread,
                [WeakSelf, LocalLiving = MoveTemp(LocalLiving),
                TraitPoolSnapshot = MoveTemp(TraitPoolSnapshot), GroupPoolSnapshot = MoveTemp(GroupPoolSnapshot), TraitKeysSnapshot = MoveTemp(TraitKeysSnapshot),
                MaleNamesSnapshot = MoveTemp(MaleNamesSnapshot), FemaleNamesSnapshot = MoveTemp(FemaleNamesSnapshot),
                LocalYear, YearsToAdvance, ThreadSettings]() mutable
                {
                    TArray<FCharacterData> SimulationDeceased;

                    for (int32 i = 0; i < YearsToAdvance; ++i)
                    {
                        LocalYear++;
                        TSet<FGuid> DeceasedIDsThisYear;
                        TMap<FGuid, FString> DeathDates;
                        TArray<TTuple<FGuid, FGuid, FName>> MarriagesThisYear;
                        TArray<FCharacterData> NewChildrenThisYear;

                        if (ULineageAIPopulationSubsystem* StrongSelf = WeakSelf.Get())
                        {
                            StrongSelf->ProcessMortality(LocalLiving, LocalYear, ThreadSettings, DeceasedIDsThisYear, DeathDates);
                            StrongSelf->ProcessMarriages(LocalLiving, LocalYear, ThreadSettings, DeceasedIDsThisYear, MarriagesThisYear);
                            StrongSelf->ProcessFertility(LocalLiving, LocalYear, ThreadSettings, DeceasedIDsThisYear, TraitPoolSnapshot, GroupPoolSnapshot, MaleNamesSnapshot, FemaleNamesSnapshot, NewChildrenThisYear);
                        }

                        // Apply the computed results securely within the background thread
                        // (Event generation logic omitted for showcase brevity)
                    }

                    if (ULineageAIPopulationSubsystem* StrongSelf = WeakSelf.Get())
                    {
                        StrongSelf->CommitSimulationToDatabase_ThreadSafe(LocalLiving, SimulationDeceased, LocalYear);
                    }

                    // Safely return the processed state to the Main Game Thread
                    AsyncTask(ENamedThreads::GameThread, [WeakSelf, LocalLiving = MoveTemp(LocalLiving), LocalYear]() mutable
                        {
                            if (ULineageAIPopulationSubsystem* StrongSelf = WeakSelf.Get()) {
                                StrongSelf->LivingCharacters = MoveTemp(LocalLiving);
                                StrongSelf->WorldYear = LocalYear;
                                StrongSelf->OnAsyncOperationFinished.Broadcast(ELineageJobType::Simulation, true, TEXT("Simulation Complete"));
                                StrongSelf->ProcessNextJob();
                            }
                        });
                });
        });
}

void ULineageAIPopulationSubsystem::CommitSimulationToDatabase_ThreadSafe(TMap<FGuid, FCharacterData>& LocalLiving, const TArray<FCharacterData>& SimulationDeceased, int32 LocalYear) const
{
    ULineageAIDatabaseSubsystem* DB = GetDatabaseSubsystem();
    if (!DB) return;

    DB->ExecuteQuery(TEXT("DROP INDEX IF EXISTS idx_char_isalive;"));
    DB->ExecuteQuery(TEXT("DROP INDEX IF EXISTS idx_char_ui_covering;"));
    DB->ExecuteQuery(TEXT("BEGIN TRANSACTION;"));

    TArray<const FCharacterData*> AllCharactersToSave;
    AllCharactersToSave.Reserve(LocalLiving.Num() + SimulationDeceased.Num());

    for (auto& Pair : LocalLiving)
    {
        AllCharactersToSave.Add(&Pair.Value);
        Pair.Value.bIsDirty = false;
        Pair.Value.bIsNewInDB = false;
    }
    for (const FCharacterData& DeadChar : SimulationDeceased)
    {
        AllCharactersToSave.Add(&DeadChar);
    }

    DB->AddOrUpdateCharacterInDatabase_Internal(AllCharactersToSave);
    DB->SaveWorldTime(LocalYear, 1, 1, 8, 0);

    DB->ExecuteQuery(TEXT("COMMIT;"));
    DB->ExecuteQuery(TEXT("CREATE INDEX IF NOT EXISTS idx_char_isalive ON Characters(IsAlive);"));
    DB->ExecuteQuery(TEXT("CREATE INDEX IF NOT EXISTS idx_char_ui_covering ON Characters(Surname, FirstName, Age, Gender, IsAlive, CultureTag, TraitTags, ID);"));
}

ULineageAIDatabaseSubsystem* ULineageAIPopulationSubsystem::GetDatabaseSubsystem() const
{
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance()) return GI->GetSubsystem<ULineageAIDatabaseSubsystem>();
    }
    return nullptr;
}