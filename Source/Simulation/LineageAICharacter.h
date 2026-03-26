/**
 * @brief Core data structure representing a single entity within the LineageAI simulation.
 */
USTRUCT(BlueprintType)
struct LINEAGEAICORE_API FCharacterData
{
    GENERATED_BODY()

    /** @brief The globally unique identifier for this character. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FGuid ID;

    /** @brief The GUID of this character's current spouse. Invalid if unmarried. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relations")
    FGuid SpouseID;

    /** @brief Array of GUIDs pointing to this character's biological parents. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relations")
    TArray<FGuid> Parents;

    /** @brief Array of GUIDs pointing to this character's children. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relations")
    TArray<FGuid> Children;

    /** @brief Array of GUIDs pointing to this character's siblings. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relations")
    TArray<FGuid> Siblings;

    /** @brief Array of internal trait names currently active on this character. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traits")
    TArray<FName> Traits;

    /** @brief Chronological list of major life events generated over the character's lifetime. */
    UPROPERTY(BlueprintReadOnly, Category = "LineageAI|Memory")
    TArray<FString> LifeEventLog;

    /** @brief Transient log of minor daily occurrences. Wiped at the end of each simulated day. */
    UPROPERTY(BlueprintReadOnly, Category = "LineageAI|Memory")
    TArray<FString> DailyEventLog;

    /** @brief LLM-generated summaries of past direct conversations with the player. */
    UPROPERTY(BlueprintReadOnly, Category = "LineageAI|Memory")
    TArray<FString> ConversationSummaryLog;

    /** @brief Flexible key-value store for plugin extensions and location tracking. */
    UPROPERTY(BlueprintReadOnly, Category = "LineageAI|Data")
    TMap<FString, FString> CustomData;

    /** @brief The character's given name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName FirstName;

    /** @brief The character's family name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName Surname;

    /** @brief Identifier linking the character to specific cultural naming or behavioral rules. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FName CultureTag;

    /** @brief Current age in simulated years. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats", meta = (ClampMin = "0"))
    int32 Age = 0;

    /** @brief The gender identity used for simulation bounds and LLM context. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    ECharacterGender Gender = ECharacterGender::Male;

    /** @brief True if the character is currently alive in the simulation. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
    bool bIsAlive = true;

    /** @brief True if a 3D physical representation of this character currently exists in the world. */
    UPROPERTY(BlueprintReadWrite, Category = "State")
    bool bIsSpawned = false;

    /** @brief Internal flag marking if the character has been modified and needs a database write. */
    UPROPERTY(BlueprintReadWrite, Transient, Category = "LineageAI")
    bool bIsDirty = true;

    /** @brief Internal flag marking if the character has never been written to the database. */
    UPROPERTY(BlueprintReadWrite, Transient, Category = "LineageAI")
    mutable bool bIsNewInDB = true;

    FCharacterData() : ID(FGuid()), SpouseID(FGuid()) {}

    FCharacterData(bool InIsAlive, FName InFirstName, FName InSurname, int32 InAge, ECharacterGender InGender, FName InCultureTag, const TArray<FName>& InTraits)
        : ID(FGuid::NewGuid()), SpouseID(FGuid()), Traits(InTraits), FirstName(InFirstName), Surname(InSurname), CultureTag(InCultureTag), Age(InAge), Gender(InGender), bIsAlive(InIsAlive)
    {
        Parents.Reserve(2);
    }

    bool HasValidId() const { return ID.IsValid(); }
    bool HasSpouse()  const { return SpouseID.IsValid(); }

    /** 
     * @brief Compresses nested arrays into binary format for SQLite BLOB storage.
     * @param Ar The archive object utilized for reading or writing.
     * @return True if serialization completes successfully.
     */
    bool SerializeComplexData(FArchive& Ar)
    {
        Ar << SpouseID;
        Ar << Parents;
        Ar << Children;
        Ar << Siblings;
        Ar << Traits;
        Ar << LifeEventLog;
        Ar << DailyEventLog;
        Ar << ConversationSummaryLog;

        if (Ar.IsSaving())
        {
            int32 MapSize = CustomData.Num();
            Ar << MapSize;
            for (auto& Pair : CustomData)
            {
                Ar << Pair.Key;
                Ar << Pair.Value;
            }
        }
        else
        {
            int32 MapSize;
            Ar << MapSize;
            CustomData.Empty();
            for (int32 i = 0; i < MapSize; ++i)
            {
                FString K, V;
                Ar << K;
                Ar << V;
                CustomData.Add(K, V);
            }
        }
        return true;
    }
};