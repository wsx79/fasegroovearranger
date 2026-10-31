// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "ProjectIO.h"
#include <functional>
namespace fga {
// IDs are stored in preferences. Append services; do not reorder existing IDs.
enum class AiProvider { openai, anthropic, gemini, ollama, deepseek, groq, openrouter, count };
juce::StringArray aiSuggestedModels(AiProvider);
juce::String aiSetupUrl(AiProvider);
struct AiOptions {
    AiProvider provider=AiProvider::ollama;
    juce::String model,key,endpoint="http://127.0.0.1:11434",prompt;
    bool wholeSong=true;
    double seconds=0; // zero: requested in the prompt, otherwise six minutes by default
    std::vector<int> tracks;
};
struct AiHttpRequest {juce::String url,headers,body;};
juce::String aiSystemPrompt();
juce::String aiContext(const Project&,const AiOptions&);
juce::Result aiHttpRequest(const Project&,const AiOptions&,AiHttpRequest&);
juce::Result aiResponse(AiProvider,const juce::var&,juce::String&);
juce::Result aiOllamaEndpoint(const juce::String&,juce::String&);
juce::Result aiOllamaModels(const juce::String&,juce::StringArray&,const std::function<bool()>& cancelled);
juce::Result aiModelsRequest(const AiOptions&,const juce::String& cursor,AiHttpRequest&);
juce::Result aiReadModels(AiProvider,const juce::var&,juce::StringArray&,juce::String& nextCursor);
// Optional catalogue transport enables deterministic pagination/error tests;
// the application uses the fixed-origin HTTPS transport when it is omitted.
using AiCatalogueFetch=std::function<juce::Result(const AiHttpRequest&,juce::var&)>;
juce::Result aiModels(const AiOptions&,juce::StringArray&,const std::function<bool()>& cancelled,const AiCatalogueFetch& readCatalogue={});
juce::Result aiCompile(const Project&,const AiOptions&,const juce::String&,Project&,juce::String& summary);
// No credentials, paths, plugin state or audio are sent in the musical context.
// The worker is cancellable; it never mutates the live project.
juce::Result aiGenerate(const Project&,const AiOptions&,Project&,juce::String&,const std::function<bool()>& cancelled);
}
