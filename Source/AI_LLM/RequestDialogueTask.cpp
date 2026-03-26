// Initialization logic omitted for showcase brevity...

void URequestDialogueTask::Activate()
{
    // Evaluates context window size to prevent token-limit overflows
    constexpr int32 ChatBlockTriggerSize = 50;
    constexpr int32 MessagesToSummarize = 25;

    int32 NumSummaries = 0;
    for (int32 i = 1; i < History.Num(); ++i)
    {
        if (History[i].role == TEXT("system")) NumSummaries++;
    }

    const int32 ChatStartIndex = 1 + NumSummaries;
    const int32 NumChatMessages = History.Num() - ChatStartIndex;

    // Trigger context compression if chat history is too large
    if (NumChatMessages >= ChatBlockTriggerSize)
    {
        TArray<FOllamaChatMessage> HistoryToSummarize;
        const int32 SummaryEndIndex = ChatStartIndex + MessagesToSummarize - 1;

        for (int32 i = ChatStartIndex; i <= SummaryEndIndex; ++i)
        {
            HistoryToSummarize.Add(History[i]);
        }

        CachedLLMSubsystem->RequestChatCompression(
            Profile.Self,
            HistoryToSummarize,
            FOllamaCompressionRequestCompleted::CreateUObject(this, &URequestDialogueTask::OnMidConversationCompressionReceived)
        );
    }
    else
    {
        // Otherwise, stream response directly to UI
        FOllamaChatMessage UserMessage;
        UserMessage.role = TEXT("user");
        UserMessage.content = NewUserMessage;
        History.Add(UserMessage);

        CachedLLMSubsystem->RequestOllamaChatCompletion(
            History,
            FOllamaChatRequestCompleted::CreateUObject(this, &URequestDialogueTask::OnChatRequestCompleted),
            FOllamaStreamUpdate::CreateLambda([this](const FString& Token)
                {
                    FStreamChunk Chunk;
                    Chunk.Text = Token;
                    OnStreamUpdate.Broadcast(FOllamaChatMessage(), TArray<FOllamaChatMessage>(), Chunk);
                })
        );
    }
}

void URequestDialogueTask::OnMidConversationCompressionReceived(bool bSuccess, const FString& CompressedText)
{
    // Appends the newly generated memory summary to the system context
    // and resumes the delayed chat request seamlessly.
    // Logic omitted for showcase brevity...
}