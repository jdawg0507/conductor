#pragma once
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <juce_core/juce_core.h>
#include "../Audio/NamChain.h"
#include "Tone3000Catalog.h"
#include "ModelCache.h"
#include "../UI/DebugLog.h"
#include "json.hpp"

namespace conductor {

struct RigSetupResult {
    bool success{false};
    bool isEditMode{false};
    bool isFallback{false};
    std::string fallbackReason;
    std::string summary;
    std::string rationale;
    std::vector<std::string> matchedTags;
    std::vector<std::string> candidateGear;
    std::string pedal1{"None"};
    std::string pedal2{"None"};
    std::string amp{"None"};
    std::string space{"None"};
};

class LLMClient {
public:
    using RigCompletionCallback = std::function<void(const RigSetupResult&)>;

    LLMClient(NamChain& chain, const std::string& workspaceRoot)
        : mChain(chain), mWorkspaceRoot(workspaceRoot)
    {
        loadEnvConfig();
    }

    ~LLMClient() {
        if (mWorkerThread.joinable()) {
            mWorkerThread.join();
        }
    }

    void submitPromptAsync(const std::string& userPrompt, bool isEditMode, RigCompletionCallback callback) {
        Log("AI", "User prompt received (" + std::string(isEditMode ? "EDIT TONE" : "NEW TONE") + "): \"" + userPrompt + "\"");

        if (mWorkerThread.joinable()) {
            mWorkerThread.join();
        }

        mWorkerThread = std::thread([this, userPrompt, isEditMode, callback]() {
            RigSetupResult result = executePrompt(userPrompt, isEditMode);

            Log("AI", "Rig configuration complete: " + result.summary);

            if (callback) {
                callback(result);
            }
        });
    }

private:
    void loadEnvConfig() {
        std::vector<std::string> envPaths = {
            mWorkspaceRoot + "/.env",
            std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/Conductor/.env",
            std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.env"
        };

        for (const auto& path : envPaths) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto eqPos = line.find('=');
                    if (eqPos != std::string::npos) {
                        std::string key = line.substr(0, eqPos);
                        std::string val = line.substr(eqPos + 1);
                        if (!val.empty() && (val.front() == '"' || val.front() == '\'')) val.erase(0, 1);
                        if (!val.empty() && (val.back() == '"' || val.back() == '\'')) val.pop_back();

                        if (key == "NEBIUS_API_KEY") mNebiusApiKey = val;
                        else if (key == "NEBIUS_BASE_URL") mNebiusBaseUrl = val;
                        else if (key == "NEBIUS_LLM_MODEL") mNebiusModel = val;
                        else if (key == "TONE3000_API_KEY") mTone3000ApiKey = val;
                        else if (key == "TONE3000_BASE_URL") mTone3000BaseUrl = val;
                    }
                }
                Log("AI", "Loaded configuration from: " + path);
                Log("AI", "LLM Model: " + (mNebiusModel.empty() ? "deepseek-ai/DeepSeek-V4.1-Flash" : mNebiusModel));
                if (!mTone3000ApiKey.empty()) {
                    Log("AI", "Tone3000 API integration authenticated.");
                }
                return;
            }
        }
        Log("AI", "Notice: No .env configuration found. Using deterministic local tag matching.");
    }

    RigSetupResult executePrompt(const std::string& prompt, bool isEditMode) {
        if (!mNebiusApiKey.empty()) {
            try {
                Log("AI", "Dispatching to Nebius AI Studio (" + (mNebiusBaseUrl.empty() ? "https://api.studio.nebius.ai/v1" : mNebiusBaseUrl) + ")...");
                RigSetupResult nebiusResult = callNebiusApi(prompt, isEditMode);
                if (nebiusResult.success) {
                    return nebiusResult;
                }
            } catch (const std::exception& e) {
                Log("AI", "Nebius request exception: " + std::string(e.what()) + ". Falling back to local tag engine.");
            }
        } else {
            Log("AI", "Notice: NEBIUS_API_KEY not set. Using local tag-matching engine.");
        }

        Log("AI", "Evaluating prompt using local TONE3000 semantic tag matching engine...");
        RigSetupResult localResult = planRigLocal(prompt, isEditMode);
        localResult.isFallback = true;
        localResult.fallbackReason = mNebiusApiKey.empty() ? "NEBIUS_API_KEY unconfigured" : "Nebius API connection failed";
        return localResult;
    }

    RigSetupResult callNebiusApi(const std::string& userPrompt, bool isEditMode) {
        std::string baseUrl = mNebiusBaseUrl.empty() ? "https://api.studio.nebius.ai/v1" : mNebiusBaseUrl;
        std::string urlStr = baseUrl + "/chat/completions";
        std::string modelStr = mNebiusModel.empty() ? "deepseek-ai/DeepSeek-V4.1-Flash" : mNebiusModel;

        std::string inventoryPrompt = buildTone3000InventoryPrompt();

        std::string currentRigContext;
        if (isEditMode) {
            std::ostringstream ss;
            ss << "[CURRENT ACTIVE RIG STATE (EDIT MODE)]\n";
            ss << "• Slot 0 (Pedal 1): " << mChain.getSlotConfig(0).name 
               << (mChain.getSlotConfig(0).isBypassed.load() ? " [Bypassed]" : " [Active]")
               << ", Gain: " << mChain.getSlotConfig(0).gainDb.load() << " dB, Mix: " << mChain.getSlotConfig(0).mix.load() << "\n";
            ss << "• Slot 1 (Pedal 2): " << mChain.getSlotConfig(1).name 
               << (mChain.getSlotConfig(1).isBypassed.load() ? " [Bypassed]" : " [Active]")
               << ", Gain: " << mChain.getSlotConfig(1).gainDb.load() << " dB, Mix: " << mChain.getSlotConfig(1).mix.load() << "\n";
            ss << "• Slot 2 (Amp Core): " << mChain.getSlotConfig(2).name 
               << ", Gain: " << mChain.getSlotConfig(2).gainDb.load() << " dB"
               << ", Treble: " << mChain.getSlotConfig(2).trebleDb.load() << " dB"
               << ", Mid: " << mChain.getSlotConfig(2).midDb.load() << " dB"
               << ", Bass: " << mChain.getSlotConfig(2).bassDb.load() << " dB\n";
            ss << "• Slot 3 (Space/Reverb): " << mChain.getSlotConfig(3).name 
               << (mChain.getSlotConfig(3).isBypassed.load() ? " [Bypassed]" : " [Active]")
               << ", Mix: " << mChain.getSlotConfig(3).mix.load() << "\n\n";
            currentRigContext = ss.str();
        }

        std::string systemPrompt;
        if (isEditMode) {
            systemPrompt =
                "You are Conductor AI, an elite guitar tone architect modifying an EXISTING ACTIVE GUITAR RIG.\n\n"
                + currentRigContext
                + inventoryPrompt + "\n"
                "EDIT MODE RULES:\n"
                "1. Interpret the user's conversational tweak request (e.g., 'more gain and mids', 'swap overdrive for a fuzz', 'cut reverb mix', 'add a tape delay').\n"
                "2. Match Tone3000 tags relevant to the requested sonic change.\n"
                "3. Compile candidate gear for any gear changes or replacements.\n"
                "4. Keep existing amp and pedals UNCHANGED unless the user specifically asks to change, replace, remove, or add them.\n"
                "5. Adjust knob settings (gain, bass, mid, treble, presence, mix) to satisfy the tweak.\n"
                "6. Respond with RAW JSON ONLY (no markdown fences, no explanatory prose outside JSON):\n"
                "{\n"
                "  \"matched_tags\": [\"tag1\", \"tag2\"],\n"
                "  \"candidate_gear\": [\"Gear Name 1\", \"Gear Name 2\"],\n"
                "  \"sound_design_rationale\": \"...\",\n"
                "  \"amp\": \"amp_id\",\n"
                "  \"pedal1\": \"pedal_id or null\",\n"
                "  \"pedal2\": \"pedal_id or null\",\n"
                "  \"space\": \"space_id or null\",\n"
                "  \"amp_knobs\": { \"gain\": 6.5, \"bass\": 5.0, \"mid\": 6.0, \"treble\": 7.0, \"presence\": 6.5 },\n"
                "  \"pedal1_knobs\": { \"gain\": 0.0, \"mix\": 1.0 },\n"
                "  \"pedal2_knobs\": { \"gain\": 0.0, \"mix\": 0.4 },\n"
                "  \"space_knobs\": { \"mix\": 0.25 }\n"
                "}";
        } else {
            systemPrompt = 
                "You are Conductor AI, an elite guitar tone architect and audio engineer.\n"
                "Your task is to analyze the user's tone prompt, parse musical intent, playing style, dynamics, artist/band/song references, and sonic descriptors, and synthesize a complete custom guitar signal chain from the TONE3000 catalog.\n\n"
                + inventoryPrompt + "\n"
                "ARCHITECTURE WORKFLOW:\n"
                "1. \"matched_tags\": Array of specific tags from the TONE3000 taxonomy connected to the prompt.\n"
                "2. \"candidate_gear\": Array of candidate gear items evaluated from the catalog.\n"
                "3. \"amp\": REQUIRED. Exactly 1 amplifier model ID from the catalog as core foundation.\n"
                "4. \"pedal1\" & \"pedal2\": AT YOUR CREATIVE DISCRETION (0, 1, or 2 pedals). Set null if pure amp tone is best.\n"
                "5. \"space\": AT YOUR CREATIVE DISCRETION (1 space ID or null for dry in-your-face tone).\n"
                "6. \"amp_knobs\": Stock/curated settings (0.0 to 10.0) for \"gain\", \"bass\", \"mid\", \"treble\", \"presence\".\n"
                "7. \"pedal1_knobs\", \"pedal2_knobs\", \"space_knobs\": Stock settings.\n"
                "8. \"sound_design_rationale\": Explain why this gear and these tags were chosen to craft the user's sound.\n\n"
                "OUTPUT FORMAT:\n"
                "Respond with RAW JSON ONLY (no markdown code blocks, no other text):\n"
                "{\n"
                "  \"matched_tags\": [\"tag1\", \"tag2\"],\n"
                "  \"candidate_gear\": [\"Gear Name 1\", \"Gear Name 2\"],\n"
                "  \"sound_design_rationale\": \"...\",\n"
                "  \"amp\": \"amp_id\",\n"
                "  \"pedal1\": \"pedal_id or null\",\n"
                "  \"pedal2\": \"pedal_id or null\",\n"
                "  \"space\": \"space_id or null\",\n"
                "  \"amp_knobs\": { \"gain\": 6.5, \"bass\": 5.0, \"mid\": 6.0, \"treble\": 7.0, \"presence\": 6.5 },\n"
                "  \"pedal1_knobs\": { \"gain\": 0.0, \"mix\": 1.0 },\n"
                "  \"pedal2_knobs\": { \"gain\": 0.0, \"mix\": 0.4 },\n"
                "  \"space_knobs\": { \"mix\": 0.25 }\n"
                "}";
        }

        nlohmann::json sysMsg;
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;

        nlohmann::json userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = userPrompt;

        nlohmann::json reqBody;
        reqBody["model"] = modelStr;
        reqBody["messages"] = nlohmann::json::array({sysMsg, userMsg});
        reqBody["temperature"] = 0.3;
        std::string reqBodyStr = reqBody.dump();

        juce::URL url(urlStr);
        juce::StringPairArray headers;
        headers.set("Content-Type", "application/json");
        headers.set("Authorization", "Bearer " + juce::String(mNebiusApiKey));

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                            .withExtraHeaders("Content-Type: application/json\r\nAuthorization: Bearer " + juce::String(mNebiusApiKey))
                            .withConnectionTimeoutMs(8000)
                            .withHttpRequestCmd("POST");

        auto urlWithData = url.withPOSTData(reqBodyStr);
        std::unique_ptr<juce::InputStream> stream(urlWithData.createInputStream(options));

        if (!stream) {
            Log("AI", "Nebius connection timed out or failed to connect to " + urlStr);
            return {false, isEditMode, true, "Connection failed or timed out", "", "", {}, {}, "", "", "", ""};
        }

        juce::String responseText = stream->readEntireStreamAsString();
        Log("AI", "Nebius HTTP Response received (" + std::to_string(responseText.length()) + " bytes)");

        auto respJson = nlohmann::json::parse(responseText.toStdString(), nullptr, false);
        if (respJson.is_discarded() || !respJson.contains("choices") || respJson["choices"].empty()) {
            Log("AI", "Invalid JSON from Nebius: " + responseText.toStdString());
            return {false, isEditMode, true, "Invalid JSON from Nebius", "", "", {}, {}, "", "", "", ""};
        }

        std::string contentStr = respJson["choices"][0]["message"]["content"].get<std::string>();
        Log("AI", "Nebius Content: " + contentStr);

        size_t jsonStart = contentStr.find('{');
        size_t jsonEnd = contentStr.rfind('}');
        if (jsonStart == std::string::npos || jsonEnd == std::string::npos) {
            return {false, isEditMode, true, "No JSON object found in response", "", "", {}, {}, "", "", "", ""};
        }

        std::string cleanJson = contentStr.substr(jsonStart, jsonEnd - jsonStart + 1);
        auto gearJson = nlohmann::json::parse(cleanJson, nullptr, false);
        if (gearJson.is_discarded()) {
            return {false, isEditMode, true, "JSON parsing error in gear payload", "", "", {}, {}, "", "", "", ""};
        }

        auto getJsonStr = [](const nlohmann::json& j, const std::string& key, const std::string& fallback) -> std::string {
            if (!j.contains(key) || j[key].is_null()) return fallback;
            if (j[key].is_string()) return j[key].get<std::string>();
            return fallback;
        };

        std::string ampId = getJsonStr(gearJson, "amp", "fender_twin_65");
        std::string p1Id = getJsonStr(gearJson, "pedal1", "");
        std::string p2Id = getJsonStr(gearJson, "pedal2", "");
        std::string spaceId = getJsonStr(gearJson, "space", "abbey_road_room");
        std::string rationale = getJsonStr(gearJson, "sound_design_rationale", "");
        if (rationale.empty()) rationale = getJsonStr(gearJson, "explanation", "");

        std::vector<std::string> tags;
        if (gearJson.contains("matched_tags") && gearJson["matched_tags"].is_array()) {
            for (const auto& t : gearJson["matched_tags"]) {
                if (t.is_string()) tags.push_back(t.get<std::string>());
            }
        }

        std::vector<std::string> candidates;
        if (gearJson.contains("candidate_gear") && gearJson["candidate_gear"].is_array()) {
            for (const auto& c : gearJson["candidate_gear"]) {
                if (c.is_string()) candidates.push_back(c.get<std::string>());
            }
        }

        if (!tags.empty()) {
            std::string tagsStr;
            for (size_t i = 0; i < tags.size(); ++i) tagsStr += (i > 0 ? ", " : "") + tags[i];
            Log("AI", "Matched Tone3000 Tags: [" + tagsStr + "]");
        }
        if (!candidates.empty()) {
            std::string candStr;
            for (size_t i = 0; i < candidates.size(); ++i) candStr += (i > 0 ? ", " : "") + candidates[i];
            Log("AI", "Candidate Gear: [" + candStr + "]");
        }
        if (!rationale.empty()) {
            Log("AI", "Design Rationale: " + rationale);
        }

        RigSetupResult result = applyGearIds(ampId, p1Id, p2Id, spaceId, gearJson);
        result.isEditMode = isEditMode;
        result.isFallback = false;
        result.rationale = rationale;
        result.matchedTags = tags;
        result.candidateGear = candidates;
        return result;
    }

    RigSetupResult planRigLocal(const std::string& prompt, bool isEditMode) {
        std::string p = prompt;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);

        const auto& catalog = getTone3000Catalog();

        if (isEditMode) {
            // Conversational knob & tweak mutation on active rig
            Log("AI", "Local Edit Mode: Tweaking active rig parameters for: \"" + prompt + "\"");
            
            // Gain adjustment
            if (p.find("more gain") != std::string::npos || p.find("heavier") != std::string::npos || p.find("more crunch") != std::string::npos) {
                float cur = mChain.getSlotConfig(2).gainDb.load();
                mChain.getSlotConfig(2).gainDb.store(std::min(12.0f, cur + 3.0f));
                Log("AI", "Tweaked Amp Gain: +" + std::to_string(3.0f) + " dB");
            } else if (p.find("less gain") != std::string::npos || p.find("cleaner") != std::string::npos) {
                float cur = mChain.getSlotConfig(2).gainDb.load();
                mChain.getSlotConfig(2).gainDb.store(std::max(-18.0f, cur - 3.0f));
                Log("AI", "Tweaked Amp Gain: -" + std::to_string(3.0f) + " dB");
            }

            // Treble / Brightness
            if (p.find("bright") != std::string::npos || p.find("more treble") != std::string::npos) {
                float cur = mChain.getSlotConfig(2).trebleDb.load();
                mChain.getSlotConfig(2).trebleDb.store(std::min(9.0f, cur + 2.5f));
            } else if (p.find("dark") != std::string::npos || p.find("warm") != std::string::npos) {
                float cur = mChain.getSlotConfig(2).trebleDb.load();
                mChain.getSlotConfig(2).trebleDb.store(std::max(-9.0f, cur - 2.5f));
            }

            // Reverb / Delay / Space
            if (p.find("more reverb") != std::string::npos || p.find("more wet") != std::string::npos || p.find("ambient") != std::string::npos) {
                float cur = mChain.getSlotConfig(3).mix.load();
                mChain.getSlotConfig(3).mix.store(std::min(0.85f, cur + 0.15f));
                mChain.getSlotConfig(3).isBypassed.store(false);
            } else if (p.find("less reverb") != std::string::npos || p.find("dry") != std::string::npos) {
                float cur = mChain.getSlotConfig(3).mix.load();
                mChain.getSlotConfig(3).mix.store(std::max(0.0f, cur - 0.15f));
            }

            RigSetupResult res;
            res.success = true;
            res.isEditMode = true;
            res.isFallback = true;
            res.fallbackReason = "Local parameter editor";
            res.amp = mChain.getSlotConfig(2).name;
            res.pedal1 = mChain.getSlotConfig(0).name;
            res.pedal2 = mChain.getSlotConfig(1).name;
            res.space = mChain.getSlotConfig(3).name;
            res.summary = res.amp + " [Tweaked parameters]";
            res.rationale = "Locally updated tone parameters according to: \"" + prompt + "\"";
            return res;
        }

        // Full New Tone Synthesizer
        auto scoreItem = [&p](const CatalogItem& item) -> float {
            float score = 0.0f;
            for (const auto& tag : item.tags) {
                if (p.find(tag) != std::string::npos) {
                    score += 4.0f;
                }
            }
            std::string lowerName = item.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (p.find(lowerName) != std::string::npos || p.find(item.id) != std::string::npos) {
                score += 5.0f;
            }
            std::string lowerMfg = item.manufacturer;
            std::transform(lowerMfg.begin(), lowerMfg.end(), lowerMfg.begin(), ::tolower);
            if (p.find(lowerMfg) != std::string::npos) {
                score += 2.5f;
            }
            std::string lowerCat = item.category;
            std::transform(lowerCat.begin(), lowerCat.end(), lowerCat.begin(), ::tolower);
            if (p.find(lowerCat) != std::string::npos) {
                score += 2.0f;
            }
            return score;
        };

        // 1. Pick highest scoring Amplifier (mandatory)
        std::string bestAmpId = "fender_twin_65";
        float bestAmpScore = -1.0f;
        for (const auto& item : catalog) {
            if (item.gearType != "Amp") continue;
            float s = scoreItem(item);
            if (s > bestAmpScore) {
                bestAmpScore = s;
                bestAmpId = item.id;
            }
        }

        // 2. Score and pick Pedals (threshold >= 3.5, max 2)
        struct ScoredPedal {
            std::string id;
            float score;
        };
        std::vector<ScoredPedal> pedalCandidates;
        for (const auto& item : catalog) {
            if (item.gearType != "Stompbox") continue;
            float s = scoreItem(item);
            if (s >= 3.5f) {
                pedalCandidates.push_back({item.id, s});
            }
        }
        std::sort(pedalCandidates.begin(), pedalCandidates.end(), [](const ScoredPedal& a, const ScoredPedal& b) {
            return a.score > b.score;
        });

        std::string p1Id = pedalCandidates.size() > 0 ? pedalCandidates[0].id : "";
        std::string p2Id = pedalCandidates.size() > 1 ? pedalCandidates[1].id : "";

        // 3. Score and pick Space (threshold >= 2.5)
        std::string bestSpaceId = "abbey_road_room";
        float bestSpaceScore = -1.0f;
        for (const auto& item : catalog) {
            if (item.gearType != "Space") continue;
            float s = scoreItem(item);
            if (s > bestSpaceScore) {
                bestSpaceScore = s;
                bestSpaceId = item.id;
            }
        }

        nlohmann::json emptyKnobs;
        RigSetupResult res = applyGearIds(bestAmpId, p1Id, p2Id, bestSpaceId, emptyKnobs);
        res.rationale = "Locally synthesized rig based on semantic tag matching for: \"" + prompt + "\"";
        return res;
    }

    RigSetupResult applyGearIds(const std::string& ampId, const std::string& p1Id, const std::string& p2Id, const std::string& spaceId, const nlohmann::json& gearJson) {
        const auto& catalog = getTone3000Catalog();
        const CatalogItem* chosenAmp = findItem(catalog, ampId);
        const CatalogItem* chosenPedal1 = p1Id.empty() || p1Id == "null" ? nullptr : findItem(catalog, p1Id);
        const CatalogItem* chosenPedal2 = p2Id.empty() || p2Id == "null" ? nullptr : findItem(catalog, p2Id);
        const CatalogItem* chosenSpace = spaceId.empty() || spaceId == "null" ? nullptr : findItem(catalog, spaceId);

        RigSetupResult res;
        res.success = true;

        // Slot 0: Pedal 1 (Discretionary)
        if (chosenPedal1) {
            std::string path = ModelCache::resolveModelPath(*chosenPedal1, mWorkspaceRoot, mTone3000ApiKey);
            bool ok = mChain.stageModel(0, path, chosenPedal1->name);
            Log("AI", "Slot 0 [Pedal]: " + chosenPedal1->name + (ok ? " (Loaded)" : " (Pending)"));
            
            float p1Mix = chosenPedal1->defaultMix;
            if (gearJson.contains("pedal1_knobs") && gearJson["pedal1_knobs"].contains("mix") && gearJson["pedal1_knobs"]["mix"].is_number()) {
                p1Mix = gearJson["pedal1_knobs"]["mix"].get<float>();
            }
            mChain.getSlotConfig(0).gainDb.store(chosenPedal1->defaultGainDb);
            mChain.getSlotConfig(0).outputLevelDb.store(chosenPedal1->defaultOutputDb);
            mChain.getSlotConfig(0).mix.store(p1Mix);
            mChain.getSlotConfig(0).isBypassed.store(false);
            res.pedal1 = chosenPedal1->name;
        } else {
            mChain.clearSlot(0);
            res.pedal1 = "None";
            Log("AI", "Slot 0: Empty (Pedal bypassed by design)");
        }

        // Slot 1: Pedal 2 (Discretionary)
        if (chosenPedal2) {
            std::string path = ModelCache::resolveModelPath(*chosenPedal2, mWorkspaceRoot, mTone3000ApiKey);
            bool ok = mChain.stageModel(1, path, chosenPedal2->name);
            Log("AI", "Slot 1 [Pedal]: " + chosenPedal2->name + (ok ? " (Loaded)" : " (Pending)"));
            
            float p2Mix = chosenPedal2->defaultMix;
            if (gearJson.contains("pedal2_knobs") && gearJson["pedal2_knobs"].contains("mix") && gearJson["pedal2_knobs"]["mix"].is_number()) {
                p2Mix = gearJson["pedal2_knobs"]["mix"].get<float>();
            }
            mChain.getSlotConfig(1).gainDb.store(chosenPedal2->defaultGainDb);
            mChain.getSlotConfig(1).outputLevelDb.store(chosenPedal2->defaultOutputDb);
            mChain.getSlotConfig(1).mix.store(p2Mix);
            mChain.getSlotConfig(1).isBypassed.store(false);
            res.pedal2 = chosenPedal2->name;
        } else {
            mChain.clearSlot(1);
            res.pedal2 = "None";
            Log("AI", "Slot 1: Empty (Pedal bypassed by design)");
        }

        // Slot 2: Amp (Mandatory)
        if (!chosenAmp) chosenAmp = findItem(catalog, "fender_twin_65");
        if (chosenAmp) {
            std::string path = ModelCache::resolveModelPath(*chosenAmp, mWorkspaceRoot, mTone3000ApiKey);
            bool ok = mChain.stageModel(2, path, chosenAmp->name);
            Log("AI", "Slot 2 [Amp]: " + chosenAmp->name + (ok ? " (Loaded)" : " (Pending)"));
            
            float ampGainDb = chosenAmp->defaultGainDb;
            float trebleDb = 0.0f;
            float midDb = 0.0f;
            float bassDb = 0.0f;

            if (gearJson.contains("amp_knobs") && gearJson["amp_knobs"].is_object()) {
                auto knobs = gearJson["amp_knobs"];
                if (knobs.contains("gain") && knobs["gain"].is_number()) {
                    float g = knobs["gain"].get<float>();
                    ampGainDb = (g - 5.0f) * 1.5f;
                }
                if (knobs.contains("treble") && knobs["treble"].is_number()) {
                    float t = knobs["treble"].get<float>();
                    trebleDb = (t - 5.0f) * 1.2f;
                }
                if (knobs.contains("mid") && knobs["mid"].is_number()) {
                    float m = knobs["mid"].get<float>();
                    midDb = (m - 5.0f) * 1.2f;
                }
                if (knobs.contains("bass") && knobs["bass"].is_number()) {
                    float b = knobs["bass"].get<float>();
                    bassDb = (b - 5.0f) * 1.2f;
                }
            }

            mChain.getSlotConfig(2).gainDb.store(ampGainDb);
            mChain.getSlotConfig(2).trebleDb.store(trebleDb);
            mChain.getSlotConfig(2).midDb.store(midDb);
            mChain.getSlotConfig(2).bassDb.store(bassDb);
            mChain.getSlotConfig(2).outputLevelDb.store(chosenAmp->defaultOutputDb);
            mChain.getSlotConfig(2).mix.store(chosenAmp->defaultMix);
            mChain.getSlotConfig(2).isBypassed.store(false);
            res.amp = chosenAmp->name;
        }

        // Slot 3: Space (Discretionary)
        if (chosenSpace) {
            mChain.clearSlot(3);
            mChain.getSlotConfig(3).name = chosenSpace->name;
            float spaceMix = chosenSpace->defaultMix;
            if (gearJson.contains("space_knobs") && gearJson["space_knobs"].contains("mix") && gearJson["space_knobs"]["mix"].is_number()) {
                spaceMix = gearJson["space_knobs"]["mix"].get<float>();
            }
            mChain.getSlotConfig(3).gainDb.store(chosenSpace->defaultGainDb);
            mChain.getSlotConfig(3).outputLevelDb.store(chosenSpace->defaultOutputDb);
            mChain.getSlotConfig(3).mix.store(spaceMix);
            mChain.getSlotConfig(3).isBypassed.store(false);
            Log("AI", "Slot 3 [Space]: " + chosenSpace->name + " (Reverb Acoustics Active)");
            res.space = chosenSpace->name;
        } else {
            mChain.clearSlot(3);
            res.space = "None";
            Log("AI", "Slot 3: Empty (Dry Tone)");
        }

        res.summary = res.amp;
        if (res.pedal1 != "None" && res.pedal2 != "None") {
            res.summary += " + [" + res.pedal1 + " & " + res.pedal2 + "]";
        } else if (res.pedal1 != "None") {
            res.summary += " + [" + res.pedal1 + "]";
        } else if (res.pedal2 != "None") {
            res.summary += " + [" + res.pedal2 + "]";
        }
        if (res.space != "None") {
            res.summary += " -> " + res.space;
        }
        return res;
    }

    const CatalogItem* findItem(const std::vector<CatalogItem>& catalog, const std::string& id) {
        for (const auto& item : catalog) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    NamChain& mChain;
    std::string mWorkspaceRoot;
    std::string mNebiusApiKey;
    std::string mNebiusBaseUrl;
    std::string mNebiusModel;
    std::string mTone3000ApiKey;
    std::string mTone3000BaseUrl;
    std::thread mWorkerThread;
};

} // namespace conductor
