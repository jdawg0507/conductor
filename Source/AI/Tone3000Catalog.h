#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <set>

namespace conductor {

struct CatalogItem {
    std::string id;
    std::string name;
    std::string gearType; // "Stompbox", "Amp", "Space"
    std::string category;
    std::string manufacturer;
    std::string description;
    std::vector<std::string> tags;
    float defaultGainDb;
    float defaultOutputDb;
    float defaultMix;
    std::string modelUrl;
    std::string fallbackFilename;
};

inline const std::vector<CatalogItem>& getTone3000Catalog() {
    static const std::vector<CatalogItem> catalog = {
        // =========================================================================
        // AMPLIFIERS (Core sonic foundation - Exactly 1 required)
        // =========================================================================
        {
            "marshall_jcm800", "Marshall JCM800 2203", "Amp", "High Gain", "Marshall",
            "100W British master-volume roar with cutting mid-bite. The quintessential 80s hard rock and heavy metal sound.",
            {"marshall", "jcm800", "british", "crunch", "80s", "hard rock", "heavy", "punchy", "raw", "lead", "slash", "acdc"},
            0.0f, -2.0f, 1.0f, "https://models.tone3000.com/amps/marshall_jcm800.nam", "wavenet_a1_standard.nam"
        },
        {
            "fender_twin_65", "Fender Twin Reverb '65", "Amp", "Clean", "Fender",
            "Pristine sparkling cleans with huge headroom, glassy top-end, and lush warm dynamic bloom. The definitive pedal platform.",
            {"fender", "twin reverb", "clean", "sparkling", "glassy", "headroom", "pedal platform", "warm", "surf", "country", "funk"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/amps/fender_twin_65.nam", "my_model.nam"
        },
        {
            "mesa_rectifier", "Mesa/Boogie Dual Rectifier", "Amp", "High Gain", "Mesa/Boogie",
            "Savage modern high-gain with crushing low-end punch, scooped midrange, and wall-of-sound rhythm power.",
            {"mesa", "boogie", "rectifier", "high gain", "metal", "modern", "scooped", "chug", "tight", "aggressive", "wall of sound", "nu metal"},
            0.0f, -3.0f, 1.0f, "https://models.tone3000.com/amps/mesa_rectifier.nam", "A2.nam"
        },
        {
            "vox_ac30", "Vox AC30 Top Boost", "Amp", "Clean/Crunch", "Vox",
            "Chimey British class-A top-boost with rich bell-like high frequencies, gritty compression, and articulate edge-of-breakup.",
            {"vox", "ac30", "chime", "top boost", "british", "indie", "jangly", "breakup", "beatles", "bright", "edge", "queen", "brian may"},
            0.0f, -1.0f, 1.0f, "https://models.tone3000.com/amps/vox_ac30.nam", "wavenet_a1_standard.nam"
        },
        {
            "soldano_slo100", "Soldano SLO-100", "Amp", "High Gain", "Soldano",
            "Boutique liquid lead tone with singing endless sustain, ultra-clear note articulation, and harmonic richness.",
            {"soldano", "slo100", "liquid lead", "sustain", "singing", "boutique", "solo", "80s", "high gain", "creamy", "clapton"},
            0.0f, -2.5f, 1.0f, "https://models.tone3000.com/amps/soldano_slo100.nam", "A2.nam"
        },
        {
            "evh_5150", "EVH 5150 III Stealth", "Amp", "High Gain", "EVH",
            "Brutal high-gain aggression with laser-focused tight bass, razor-sharp transients, and punishing modern palm-mute chugs.",
            {"evh", "5150", "brown sound", "metalcore", "tight", "chug", "aggressive", "high gain", "modern metal", "djent"},
            0.0f, -2.5f, 1.0f, "https://models.tone3000.com/amps/evh_5150.nam", "A2.nam"
        },
        {
            "orange_rockerverb", "Orange Rockerverb 100", "Amp", "Fuzz/Crunch", "Orange",
            "Massive thick British crunch bordering on fuzz with dark vintage warmth and huge low-mid weight.",
            {"orange", "rockerverb", "doom", "sludge", "fuzzy", "thick", "heavy", "dark", "vintage crunch", "stoner rock"},
            0.0f, -2.0f, 1.0f, "https://models.tone3000.com/amps/orange_rockerverb.nam", "wavenet_a1_standard.nam"
        },
        {
            "dumble_ods", "Dumble Overdrive Special", "Amp", "Boutique Clean/Lead", "Dumble",
            "Ultra-rare boutique touch-sensitive amplifier with silky smooth compression, harmonic blooming, and singing fusion lead.",
            {"dumble", "ods", "smooth", "fusion", "warm tube", "boutique", "dynamic", "overdrive", "mayer", "robben ford"},
            0.0f, -1.0f, 1.0f, "https://models.tone3000.com/amps/dumble_ods.nam", "my_model.nam"
        },
        {
            "roland_jc120", "Roland JC-120 Jazz Chorus", "Amp", "Pristine Clean", "Roland",
            "Ultra-clean solid-state platform with endless headroom, pristine transient response, and zero unwanted distortion.",
            {"roland", "jc120", "jazz chorus", "pristine clean", "solid state", "ambient", "shimmer", "dream pop", "post punk", "the cure"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/amps/roland_jc120.nam", "wavenet_a1_standard.nam"
        },

        // =========================================================================
        // STOMPBOX PEDALS (Free choice by AI: 0, 1, or 2 based on tone synergy)
        // =========================================================================
        {
            "ts9_tube_screamer", "Ibanez TS9 Tube Screamer", "Stompbox", "Overdrive", "Ibanez",
            "Mid-hump overdrive that cuts flubby bass and boosts punchy mids. Tightens high-gain amps or pushes tube amps into blues grit.",
            {"overdrive", "tube screamer", "ts9", "mid-hump", "boost", "tight", "blues", "srv", "tighten bass", "lead boost"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/pedals/ibanez_ts9.nam", "A2.nam"
        },
        {
            "klon_centaur", "Klon Centaur Gold", "Stompbox", "Overdrive", "Klon",
            "Legendary transparent germanium overdrive with harmonically rich headroom. Adds body and chime without masking guitar character.",
            {"klon", "centaur", "transparent", "clean boost", "overdrive", "warm", "harmonic", "dynamic", "boutique", "edge of breakup"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/pedals/klon_centaur.nam", "slimmable_container.nam"
        },
        {
            "big_muff_pi", "Electro-Harmonix Big Muff Pi", "Stompbox", "Fuzz", "Electro-Harmonix",
            "Massive violin-like sustained fuzz with scooped midrange and crushing low-end. The definitive wall-of-sound fuzz.",
            {"fuzz", "big muff", "muff", "thick", "heavy", "grunge", "doom", "stoner", "violin sustain", "wall of fuzz", "gilmour", "smashing pumpkins"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/pedals/ehx_big_muff.nam", "wavenet_condition_dsp.nam"
        },
        {
            "proco_rat", "ProCo Rat 2", "Stompbox", "Distortion", "ProCo",
            "Gritty aggressive hard-clipping distortion with an inverted filter that cuts from cutting edge to dark fuzzy saturation.",
            {"rat", "distortion", "hard clipping", "punk", "80s", "gritty", "aggressive", "raw", "fuzzy", "grunge"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/pedals/proco_rat2.nam", "A2.nam"
        },
        {
            "boss_ce2", "Boss CE-2 Chorus", "Stompbox", "Modulation", "Boss",
            "Iconic warm analog bucket-brigade chorus from 1979. Imparts luscious 80s shimmer, stereo spread, and dreamy modulation.",
            {"chorus", "analog", "shimmer", "80s", "lush", "clean", "glassy", "dreamy", "modulation", "swirl", "police", "metallica clean"},
            0.0f, 0.0f, 0.7f, "https://models.tone3000.com/pedals/boss_ce2.nam", "slimmable_container.nam"
        },
        {
            "mxr_phase90", "MXR Phase 90 Script", "Stompbox", "Modulation", "MXR",
            "Classic 4-stage analog phaser sweeping from subtle warm shimmer to deep jet-engine vortex sweep.",
            {"phaser", "swirl", "van halen", "psychedelic", "modulation", "sweeping", "jet", "script", "eddie"},
            0.0f, 0.0f, 0.65f, "https://models.tone3000.com/pedals/mxr_phase90.nam", "slimmable_container.nam"
        },
        {
            "space_echo", "Roland RE-201 Space Echo", "Stompbox", "Delay", "Roland",
            "Analog magnetic multi-head tape delay with vintage tape saturation, subtle wow & flutter, and atmospheric dub repeats.",
            {"delay", "echo", "tape echo", "repeats", "flutter", "space", "ambient", "slapback", "vintage", "analog", "pink floyd", "radiohead"},
            0.0f, 0.0f, 0.40f, "https://models.tone3000.com/pedals/re201_tape.nam", "my_model.nam"
        },
        {
            "dyna_comp", "MXR Dyna Comp", "Stompbox", "Dynamics", "MXR",
            "Percussive vintage OTA compressor adding snap, sustain, and attack to clean notes or evening out lead phrases.",
            {"compressor", "percussive", "sustain", "spank", "funk", "country", "tight attack", "squash", "chickin pickin"},
            0.0f, 0.0f, 0.8f, "https://models.tone3000.com/pedals/mxr_dyna_comp.nam", "slimmable_container.nam"
        },
        {
            "isp_decimator", "ISP Decimator II", "Stompbox", "Dynamics", "ISP Technologies",
            "Transparent high-performance noise suppressor that clamps noise, hiss, and feedback while preserving dynamic palm mutes.",
            {"noise gate", "gate", "chug", "metal", "tight", "silent", "high gain", "palm mute", "clean decay"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/pedals/isp_decimator.nam", "slimmable_container.nam"
        },

        // =========================================================================
        // SPACES / REVERBS / ROOMS (Free choice by AI: 0 or 1 space)
        // =========================================================================
        {
            "abbey_road_room", "Abbey Road Studio 2 Acoustic Room", "Space", "Room", "Abbey Road",
            "World-famous studio live room with natural warm early reflections that place the guitar in a real acoustic space without clutter.",
            {"room", "studio room", "acoustic", "natural", "early reflections", "warm", "intimate", "subtle", "live room"},
            0.0f, 0.0f, 0.25f, "https://models.tone3000.com/spaces/abbey_road_room.wav", "my_model.nam"
        },
        {
            "lexicon_224", "Lexicon 224 Digital Plate", "Space", "Reverb", "Lexicon",
            "Legendary 1978 studio digital plate reverb with deep expansive decay, lush diffusion, and classic 80s studio gloss.",
            {"reverb", "plate", "hall", "cathedral", "space", "ambient", "vast", "80s studio", "lush", "expansive"},
            0.0f, 0.0f, 0.35f, "https://models.tone3000.com/spaces/lexicon_224.wav", "wavenet_a1_standard.nam"
        },
        {
            "fender_6g15", "Fender 6G15 Tube Spring Reverb", "Space", "Reverb", "Fender",
            "Vintage standalone tube-driven drip spring reverb tank. Authentic surf splash, metallic twang, and bouncy decay.",
            {"spring", "surf", "vintage", "twang", "drip", "warm", "reverb", "60s", "dick dale"},
            0.0f, 0.0f, 0.30f, "https://models.tone3000.com/spaces/fender_6g15.wav", "wavenet_condition_dsp.nam"
        },
        {
            "cab_4x12_v30", "Marshall 1960AV 4x12 Celestion V30", "Space", "Cabinet", "Marshall",
            "Classic 4x12 closed-back cabinet mic'd with Shure SM57 on cone edge. Punchy upper midrange bite and tight focused bass response.",
            {"cabinet", "4x12", "celestion", "v30", "punchy", "focused", "stage", "tight bass", "sm57"},
            0.0f, 0.0f, 1.0f, "https://models.tone3000.com/spaces/1960av_v30_sm57.wav", "wavenet_condition_dsp.nam"
        }
    };
    return catalog;
}

inline std::string buildTone3000InventoryPrompt() {
    const auto& catalog = getTone3000Catalog();
    std::ostringstream ss;

    ss << "[TONE3000 HARDWARE INVENTORY & TAG TAXONOMY]\n\n";

    ss << "=== 1. AMPLIFIERS (RULE: MUST PICK EXACTLY 1 AMP AS CORE FOUNDATION) ===\n";
    for (const auto& item : catalog) {
        if (item.gearType != "Amp") continue;
        ss << "• ID: \"" << item.id << "\" | " << item.name << " (" << item.manufacturer << ")\n";
        ss << "  Description: " << item.description << "\n";
        ss << "  Tags: [";
        for (size_t i = 0; i < item.tags.size(); ++i) {
            ss << item.tags[i] << (i + 1 < item.tags.size() ? ", " : "");
        }
        ss << "]\n\n";
    }

    ss << "=== 2. STOMPBOX PEDALS (RULE: YOU CHOOSE 0, 1, OR 2 PEDALS BASED ON SIGNAL CHAIN SYNERGY) ===\n";
    for (const auto& item : catalog) {
        if (item.gearType != "Stompbox") continue;
        ss << "• ID: \"" << item.id << "\" | " << item.name << " (" << item.manufacturer << ")\n";
        ss << "  Description: " << item.description << "\n";
        ss << "  Tags: [";
        for (size_t i = 0; i < item.tags.size(); ++i) {
            ss << item.tags[i] << (i + 1 < item.tags.size() ? ", " : "");
        }
        ss << "]\n\n";
    }

    ss << "=== 3. SPACES & REVERBS & IRS (RULE: YOU CHOOSE 0 OR 1 SPACE, OR NULL FOR DRY DIRECT TONE) ===\n";
    for (const auto& item : catalog) {
        if (item.gearType != "Space") continue;
        ss << "• ID: \"" << item.id << "\" | " << item.name << " (" << item.manufacturer << ")\n";
        ss << "  Description: " << item.description << "\n";
        ss << "  Tags: [";
        for (size_t i = 0; i < item.tags.size(); ++i) {
            ss << item.tags[i] << (i + 1 < item.tags.size() ? ", " : "");
        }
        ss << "]\n\n";
    }

    return ss.str();
}

} // namespace conductor
