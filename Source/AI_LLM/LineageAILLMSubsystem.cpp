// Prompt Generation functions omitted for showcase brevity.
// Focus on Job Queue Architecture and Priority Routing.

void ULineageAILLMSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ActiveBackend = NewObject<UOllamaBackend>(this);
    ActiveBackend->InitializeBackend();
}

void ULineageAILLMSubsystem::Deinitialize()
{
    bIsLLMBusy = true;
    HighPriorityQueue.Empty();
    LowPriorityQueue.Empty();
    Super::Deinitialize();
}

// =====================================================================
// PRIORITY JOB QUEUE ARCHITECTURE
// =====================================================================

void ULineageAILLMSubsystem::SerializeAndQueueJob(
    const FOllamaChatRequest& RequestData,
    ELineageRequestPriority Priority,
    TFunction<void(bool, const FString&)> InternalCallback,
    FOllamaStreamUpdate StreamCallback)
{
    FString RequestBodyString;
    if (!FJsonObjectConverter::UStructToJsonObjectString(RequestData, RequestBodyString))
    {
        InternalCallback(false, TEXT("JSON Serialization Error"));
        return;
    }

    FLLMJob NewJob;
    NewJob.Priority = Priority;
    NewJob.EndpointURL = OllamaApiUrl_Chat;
    NewJob.JsonPayload = RequestBodyString;

    if (StreamCallback.IsBound())
    {
        NewJob.OnStreamUpdate = [StreamCallback](const FString& Token) { StreamCallback.ExecuteIfBound(Token); };
    }

    NewJob.OnComplete = InternalCallback;

    // Route traffic based on player urgency
    if (Priority == ELineageRequestPriority::High_Chat)
    {
        HighPriorityQueue.Add(NewJob);
    }
    else
    {
        LowPriorityQueue.Add(NewJob);
    }

    ProcessNextJob();
}

void ULineageAILLMSubsystem::ProcessNextJob()
{
    if (bIsLLMBusy) return;

    FLLMJob JobToRun;
    bool bJobFound = false;

    // Service High Priority UI Requests First
    if (HighPriorityQueue.Num() > 0)
    {
        JobToRun = HighPriorityQueue.Pop(EAllowShrinking::No);
        bJobFound = true;
    }
    // Fallback to Background Simulation Tasks
    else if (LowPriorityQueue.Num() > 0)
    {
        JobToRun = LowPriorityQueue[0];
        LowPriorityQueue.RemoveAt(0);
        bJobFound = true;
    }

    if (!bJobFound || !ActiveBackend) return;

    bIsLLMBusy = true;

    auto OriginalCallback = JobToRun.OnComplete;
    TWeakObjectPtr<ULineageAILLMSubsystem> WeakThis(this);

    // Intercept the completion to trigger the next job in the queue
    JobToRun.OnComplete = [WeakThis, OriginalCallback](bool bSuccess, const FString& Response)
        {
            AsyncTask(ENamedThreads::GameThread, [WeakThis, OriginalCallback, bSuccess, Response]()
                {
                    if (OriginalCallback) OriginalCallback(bSuccess, Response);

                    if (WeakThis.IsValid())
                    {
                        WeakThis->bIsLLMBusy = false;
                        WeakThis->ProcessNextJob();
                    }
                });
        };

    ActiveBackend->SubmitJob(JobToRun);
}