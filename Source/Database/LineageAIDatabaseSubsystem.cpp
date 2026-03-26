/**
 * @brief RAII Wrapper for SQLite Prepared Statements.
 * Guarantees memory cleanup when the statement falls out of scope, preventing memory leaks 
 * during early returns or exceptions in mass database operations.
 */
struct FSQLiteStatementWrapper
{
    sqlite3_stmt* Stmt = nullptr;

    FSQLiteStatementWrapper() = default;
    
    ~FSQLiteStatementWrapper()
    {
        if (Stmt) sqlite3_finalize(Stmt);
    }

    FSQLiteStatementWrapper(const FSQLiteStatementWrapper&) = delete;
    FSQLiteStatementWrapper& operator=(const FSQLiteStatementWrapper&) = delete;

    sqlite3_stmt** GetPtr() { return &Stmt; }
    operator sqlite3_stmt*() const { return Stmt; }
};

void ULineageAIDatabaseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FCoreDelegates::OnEnginePreExit.AddUObject(this, &ULineageAIDatabaseSubsystem::HandleEnginePreExit);
}

void ULineageAIDatabaseSubsystem::Deinitialize()
{
    HandleEnginePreExit();
    FCoreDelegates::OnEnginePreExit.RemoveAll(this);
    Super::Deinitialize();
}

void ULineageAIDatabaseSubsystem::HandleEnginePreExit()
{
    if (DatabaseHandle)
    {
        UE_LOG(LogLineageAIDatabase, Log, TEXT("Engine PreExit triggered. Closing SQLite safely to prevent corruption."));
        sqlite3_close(DatabaseHandle);
        DatabaseHandle = nullptr;
    }
}

// =====================================================================
// CONNECTION & SCHEMA INITIALIZATION
// =====================================================================

void ULineageAIDatabaseSubsystem::OpenDatabase(bool& bSuccess, FString& OutMessage, FString Filename)
{
    // STANDARD DIRECTORY CREATION LOGIC OMITTED FOR SHOWCASE
    // Handles string sanitization and creates the required folder structures before opening the DB.
    
    FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + "LineageAI/DB/", Filename);
    
    int ResultCode = sqlite3_open(TCHAR_TO_UTF8(*FullPath), &DatabaseHandle);

    if (ResultCode != SQLITE_OK)
    {
        OutMessage = FString::Printf(TEXT("Failed to open database: %s"), UTF8_TO_TCHAR(sqlite3_errmsg(DatabaseHandle)));
        DatabaseHandle = nullptr;
        bSuccess = false;
    }
    else
    {
        bSuccess = true;
        InitializeDatabaseSchema();
    }
}

void ULineageAIDatabaseSubsystem::InitializeDatabaseSchema()
{
    if (!DatabaseHandle) return;

    // Critical HPC Optimizations: Moves journals and temp stores to RAM to prevent I/O bottlenecks
    ExecuteQuery(TEXT("PRAGMA page_size = 32768;"));
    ExecuteQuery(TEXT("PRAGMA journal_mode = MEMORY;"));
    ExecuteQuery(TEXT("PRAGMA synchronous = OFF;"));
    ExecuteQuery(TEXT("PRAGMA locking_mode = EXCLUSIVE;"));
    ExecuteQuery(TEXT("PRAGMA temp_store = MEMORY;"));
    ExecuteQuery(TEXT("PRAGMA cache_size = -2000000;"));

    // STANDARD TABLE CREATION OMITTED FOR SHOWCASE
    // Creates Characters, Traits, and WorldState tables along with their respective indexes.
}

bool ULineageAIDatabaseSubsystem::ExecuteQuery(const FString& Query)
{
    if (!DatabaseHandle) return false;

    char* ErrorMessage = nullptr;
    int ResultCode = sqlite3_exec(DatabaseHandle, TCHAR_TO_UTF8(*Query), nullptr, nullptr, &ErrorMessage);

    if (ResultCode != SQLITE_OK)
    {
        sqlite3_free(ErrorMessage);
        return false;
    }
    return true;
}

// =====================================================================
// CORE DATA SERIALIZATION
// =====================================================================

bool ULineageAIDatabaseSubsystem::GetCharacterFromDatabase(const FGuid& CharacterID, FCharacterData& OutCharacter)
{
    if (!DatabaseHandle) return false;

    bool bFound = false;
    FSQLiteStatementWrapper Stmt;

    const char* Sql = "SELECT IsAlive, FirstName, Surname, Age, Gender, CultureTag, ComplexData FROM Characters WHERE ID = ?1;";

    if (sqlite3_prepare_v2(DatabaseHandle, Sql, -1, Stmt.GetPtr(), nullptr) == SQLITE_OK)
    {
        FString IDStr = CharacterID.ToString();
        FTCHARToUTF8 Utf8ID(*IDStr);
        sqlite3_bind_text(Stmt, 1, Utf8ID.Get(), -1, SQLITE_STATIC);

        if (sqlite3_step(Stmt) == SQLITE_ROW)
        {
            OutCharacter.ID = CharacterID;
            OutCharacter.bIsAlive = (sqlite3_column_int(Stmt, 0) != 0);
            
            // Raw string parsing logic omitted for showcase brevity...
            OutCharacter.Age = sqlite3_column_int(Stmt, 3);
            OutCharacter.Gender = static_cast<ECharacterGender>(sqlite3_column_int(Stmt, 4));

            // Extract the compressed binary array (BLOB) and deserialize it via FMemoryReader
            const void* BlobData = sqlite3_column_blob(Stmt, 6);
            int32 BlobSize = sqlite3_column_bytes(Stmt, 6);

            if (BlobData && BlobSize > 0)
            {
                TArray<uint8> BinaryArray;
                BinaryArray.Append((const uint8*)BlobData, BlobSize);

                FMemoryReader MemoryReader(BinaryArray, true);
                OutCharacter.SerializeComplexData(MemoryReader);
            }

            bFound = true;
        }
    }

    return bFound;
}

bool ULineageAIDatabaseSubsystem::AddOrUpdateCharacterInDatabase_Internal(const TArray<const FCharacterData*>& Characters)
{
    if (!DatabaseHandle || Characters.Num() == 0) return false;

    const char* Sql = "REPLACE INTO Characters (ID, IsAlive, FirstName, Surname, Age, Gender, CultureTag, TraitTags, ComplexData) "
                      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";

    FSQLiteStatementWrapper Stmt;
    
    if (sqlite3_prepare_v2(DatabaseHandle, Sql, -1, Stmt.GetPtr(), nullptr) != SQLITE_OK) return false;

    bool bSuccess = true;

    for (const FCharacterData* Char : Characters)
    {
        if (!Char) continue;

        // Standard integer/string binding omitted for showcase brevity...
        sqlite3_bind_int(Stmt, 2, Char->bIsAlive ? 1 : 0);
        sqlite3_bind_int(Stmt, 5, Char->Age);

        // Compress complex dynamic data into a binary array (BLOB) via FMemoryWriter
        TArray<uint8> BinaryArray;
        FMemoryWriter MemoryWriter(BinaryArray, true);

        FCharacterData MutableChar = *Char;
        MutableChar.SerializeComplexData(MemoryWriter);

        if (BinaryArray.Num() > 0)
        {
            sqlite3_bind_blob(Stmt, 9, BinaryArray.GetData(), BinaryArray.Num(), SQLITE_TRANSIENT);
        }
        else
        {
            sqlite3_bind_null(Stmt, 9);
        }

        if (sqlite3_step(Stmt) != SQLITE_DONE)
        {
            bSuccess = false;
        }

        sqlite3_reset(Stmt);
    }

    return bSuccess;
}

// =====================================================================
// EXTENDED QUERIES (OMITTED)
// =====================================================================

FPagedCharacterResult ULineageAIDatabaseSubsystem::FindCharactersByCriteria(const FCharacterQueryCriteria& Criteria, const FPaginationQuery& Paging, bool bSkipCount)
{
    // PROPRIETARY LOGIC OMITTED FOR SHOWCASE
    // Constructs dynamic, paginated SQL queries based on complex UI filter criteria 
    // (traits, age ranges, lineage, etc.) and deserializes the resulting BLOBs.
    return FPagedCharacterResult();
}

void ULineageAIDatabaseSubsystem::SyncTraitsFromDataTable(UDataTable* TraitDataTable)
{
    // CRUD OPERATIONS OMITTED FOR SHOWCASE
    // Parses Unreal Engine DataTables and pushes initial definitions into the SQLite schema.
}