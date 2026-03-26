void UOllamaBackend::InitializeBackend()
{
    UE_LOG(LogLineageAILLM, Log, TEXT("Ollama Backend Initialized. Ready for HTTP stream requests."));
}

void UOllamaBackend::SubmitJob(const FLLMJob& Job)
{
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

    HttpRequest->SetURL(Job.EndpointURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    StreamBuffer.Empty();
    LastByteProcessed = 0;

    FString FinalPayload = Job.JsonPayload;

    // Dynamically inject streaming requirement for UI-facing tasks
    if (Job.OnStreamUpdate)
    {
        FinalPayload = FinalPayload.Replace(TEXT("\"stream\": false"), TEXT("\"stream\": true"));
        FinalPayload = FinalPayload.Replace(TEXT("\"stream\":false"), TEXT("\"stream\":true"));
    }

    HttpRequest->SetContentAsString(FinalPayload);
    TWeakObjectPtr<UOllamaBackend> WeakThis(this);

    // =====================================================================
    // ASYNC HTTP STREAM PARSER
    // =====================================================================
    if (Job.OnStreamUpdate)
    {
        HttpRequest->OnRequestProgress64().BindLambda(
            [WeakThis, Job](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
            {
                if (BytesReceived == 0 || !WeakThis.IsValid() || BytesReceived <= WeakThis->LastByteProcessed) return;

                FHttpResponsePtr Response = Request->GetResponse();
                if (!Response.IsValid()) return;

                const TArray<uint8>& Content = Response->GetContent();
                const uint64 NewBytesCount = BytesReceived - WeakThis->LastByteProcessed;

                FString NewData;
                FFileHelper::BufferToString(NewData, Content.GetData() + WeakThis->LastByteProcessed, NewBytesCount);

                WeakThis->StreamBuffer += NewData;
                WeakThis->LastByteProcessed = BytesReceived;

                int32 OpenBrackets = 0;
                int32 StartIndex = -1;

                // Manually parse JSON chunks as they stream in to prevent game-thread blocking
                for (int32 i = 0; i < WeakThis->StreamBuffer.Len(); ++i)
                {
                    const TCHAR Char = WeakThis->StreamBuffer[i];

                    if (Char == '{')
                    {
                        if (OpenBrackets == 0) StartIndex = i;
                        OpenBrackets++;
                    }
                    else if (Char == '}')
                    {
                        OpenBrackets--;
                        if (OpenBrackets == 0 && StartIndex != -1)
                        {
                            const int32 Length = i - StartIndex + 1;
                            const FString JsonObjectStr = WeakThis->StreamBuffer.Mid(StartIndex, Length);

                            FOllamaChatResponse ResponseData;
                            if (FJsonObjectConverter::JsonObjectStringToUStruct(JsonObjectStr, &ResponseData, 0, 0))
                            {
                                const FString Token = ResponseData.message.content;
                                if (!Token.IsEmpty())
                                {
                                    // Push tokens to the GameThread for immediate UI rendering
                                    AsyncTask(ENamedThreads::GameThread, [Job, Token]() { Job.OnStreamUpdate(Token); });
                                }
                            }

                            WeakThis->StreamBuffer.RemoveAt(0, i + 1);
                            i = -1;
                            StartIndex = -1;
                        }
                    }
                }
            });
    }

    // =====================================================================
    // COMPLETION HANDLING
    // =====================================================================
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [WeakThis, Job](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!WeakThis.IsValid()) return;

            FString ResponseStr;
            bool bHttpSuccess = false;

            if (bWasSuccessful && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
            {
                ResponseStr = Response->GetContentAsString();
                bHttpSuccess = true;
            }
            else
            {
                ResponseStr = Response.IsValid() ? FString::Printf(TEXT("HTTP %d"), Response->GetResponseCode()) : TEXT("Connection Error");
            }

            if (Job.OnComplete)
            {
                AsyncTask(ENamedThreads::GameThread, [Job, bHttpSuccess, ResponseStr]() { Job.OnComplete(bHttpSuccess, ResponseStr); });
            }
        });

    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG(LogLineageAILLM, Error, TEXT("OllamaBackend: Failed to start HTTP Request."));
        if (Job.OnComplete)
        {
            AsyncTask(ENamedThreads::GameThread, [Job]() { Job.OnComplete(false, TEXT("Failed to create HTTP Request")); });
        }
    }
}