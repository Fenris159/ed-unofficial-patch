// Pure texture mip-bias derivation for advanced.texture_lod_bias (VR and flat).
//
// In VR, Elite's render scale is <HMDRenderTargetMultiplier> in its newest
// Options\Graphics\*.fxcfg. In flat, the render fraction is <SSAAMultiplier>.
// Keeping the parser and derivation header-only lets config_test prove both
// profiles and missing-tag edge cases without spinning up a D3D11 device.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "temporal_mode.h"

namespace edvr {

// Reads advanced.texture_lod_bias using the active profile's temporal AA default
// ("auto" when temporal AA is enabled for that profile, else "0"). Single owner
// shared by device_hook.cpp and mode_watch.cpp so live-switch checks never drift.
template <typename Cfg>
inline std::string readTextureLodBiasKey(const Cfg& cfg, bool flatProfile) {
    const std::string temporalMode =
        flatProfile ? cfg.requestedTemporalMode() : cfg.getString("fix.temporal_aa", "off");
    const bool temporal = temporalModeEnabledFor(temporalMode, flatProfile);
    return cfg.getString("advanced.texture_lod_bias", temporal ? "auto" : "0");
}

// Strips XML comments (<!-- ... -->) so commented-out tags never shadow active values.
inline std::string stripFxcfgXmlComments(const std::string& xml) {
    std::string out;
    out.reserve(xml.size());
    size_t i = 0;
    while (i < xml.size()) {
        if (xml.compare(i, 4, "<!--") == 0) {
            const size_t end = xml.find("-->", i + 4);
            if (end == std::string::npos) break;
            i = end + 3;
            continue;
        }
        out += xml[i++];
    }
    return out;
}

// Extracts trimmed <PresetName>...</PresetName> from Elite's Settings.xml (comment-stripped).
inline bool parseSettingsPresetName(const std::string& settingsXml, std::string* presetOut) {
    if (presetOut) presetOut->clear();
    const std::string clean = stripFxcfgXmlComments(settingsXml);
    const char* const openTag = "<PresetName>";
    size_t at = clean.find(openTag);
    if (at == std::string::npos) return false;
    at += std::strlen(openTag);
    const size_t end = clean.find('<', at);
    if (end == std::string::npos) return false;
    size_t a = at, b = end;
    while (a < b && (clean[a] == ' ' || clean[a] == '\t' || clean[a] == '\r' || clean[a] == '\n')) ++a;
    while (b > a && (clean[b - 1] == ' ' || clean[b - 1] == '\t' || clean[b - 1] == '\r' || clean[b - 1] == '\n')) --b;
    if (a == b) return false;
    if (presetOut) *presetOut = clean.substr(a, b - a);
    return true;
}

// Safe file-stem check matching terrain_checkerboard_reader.h: letters, digits, '_', '-', <= 32 chars.
inline bool isSafePresetStem(const std::string& name) {
    if (name.empty() || name.size() > 32) return false;
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

struct PresetFxcfgEntry {
    std::string name;
    uint64_t mtime = 0;
};

inline bool endsWithFxcfg(const std::string& name) {
    return name.size() > 6 && _strnicmp(name.c_str() + name.size() - 6, ".fxcfg", 6) == 0;
}

// Matches <prefix>.<major>.<minor>.fxcfg, <prefix>.<major>.fxcfg, or <prefix>.fxcfg (case-insensitive)
// while rejecting lookalike backups such as Custom.4.4.fxcfg-backup.
inline bool parsePresetFxcfgVersion(const std::string& fileName, const std::string& prefix,
                                    unsigned* majorOut, unsigned* minorOut) {
    const size_t sufLen = 6;  // ".fxcfg"
    if (fileName.size() < prefix.size() + sufLen) return false;
    if (_strnicmp(fileName.c_str(), prefix.c_str(), prefix.size()) != 0) return false;
    if (_strnicmp(fileName.c_str() + fileName.size() - sufLen, ".fxcfg", sufLen) != 0) return false;
    const size_t midStart = prefix.size();
    const size_t midEnd = fileName.size() - sufLen;
    if (midStart == midEnd) {
        if (majorOut) *majorOut = 0;
        if (minorOut) *minorOut = 0;
        return true;
    }
    if (fileName[midStart] != '.') return false;
    const std::string middle = fileName.substr(midStart + 1, midEnd - (midStart + 1));
    if (middle.empty()) return false;
    unsigned parts[2] = {0, 0};
    unsigned count = 0;
    size_t i = 0;
    while (i < middle.size()) {
        if (count == 2) return false;
        size_t j = i;
        unsigned long val = 0;
        while (j < middle.size() && middle[j] >= '0' && middle[j] <= '9') {
            val = val * 10 + static_cast<unsigned long>(middle[j] - '0');
            if (val > 1000000ul) return false;
            ++j;
        }
        if (j == i) return false;
        parts[count++] = static_cast<unsigned>(val);
        if (j < middle.size()) {
            if (middle[j] != '.') return false;
            ++j;
            if (j == middle.size()) return false;
        }
        i = j;
    }
    if (count == 0) return false;
    if (majorOut) *majorOut = parts[0];
    if (minorOut) *minorOut = parts[1];
    return true;
}

// Picks the highest <major>.<minor> .fxcfg matching `prefix` (newest mtime breaks ties), or -1 if none.
inline int pickPresetFxcfg(const std::string& prefix, const std::vector<PresetFxcfgEntry>& files) {
    int best = -1;
    unsigned bestMajor = 0, bestMinor = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        unsigned major = 0, minor = 0;
        if (!parsePresetFxcfgVersion(files[i].name, prefix, &major, &minor)) continue;
        const bool better = best < 0 || major > bestMajor ||
                            (major == bestMajor && minor > bestMinor) ||
                            (major == bestMajor && minor == bestMinor && files[i].mtime > files[best].mtime);
        if (better) {
            best = static_cast<int>(i);
            bestMajor = major;
            bestMinor = minor;
        }
    }
    return best;
}

// Parses <HMDRenderTargetMultiplier> and <SSAAMultiplier> out of an .fxcfg
// document (ignoring XML comments), writing valid values in (0, 8) to *hmdOut / *ssaaOut
// (0.0f when absent or invalid). Returns true when <HMDRenderTargetMultiplier> was found,
// preserving eliteHmdMultiplier()'s existing return contract for VR callers.
inline bool parseFxcfgMultipliers(const std::string& text, float* hmdOut, float* ssaaOut) {
    if (hmdOut) *hmdOut = 0.0f;
    if (ssaaOut) *ssaaOut = 0.0f;
    const std::string clean = stripFxcfgXmlComments(text);
    struct Field {
        const char* tag;
        float* out;
    };
    const Field fields[] = {
        {"<HMDRenderTargetMultiplier>", hmdOut},
        {"<SSAAMultiplier>", ssaaOut},
    };
    bool gotHmd = false;
    for (const Field& fl : fields) {
        if (!fl.out) continue;
        const size_t at = clean.find(fl.tag);
        if (at == std::string::npos) continue;
        const float v = static_cast<float>(std::atof(clean.c_str() + at + std::strlen(fl.tag)));
        if (!(v > 0.0f) || !(v < 8.0f)) continue;
        *fl.out = v;
        if (fl.out == hmdOut) gotHmd = true;
    }
    return gotHmd;
}

// Resolves Elite's active graphics preset .fxcfg using Settings.xml's <PresetName> (#253):
// - "Custom": highest-versioned Custom.<major>.<minor>.fxcfg in Options\Graphics.
// - Stock / named preset: <gameDir>\OptionDefaults\<Preset>.fxcfg first, else highest-versioned
//   <Preset>.<major>.<minor>.fxcfg in Options\Graphics; never falls back to a leftover Custom file
//   when Settings.xml names a different preset.
// - Absent / unreadable Settings.xml or missing <PresetName>: falls back to newest *.fxcfg in
//   Options\Graphics so legacy/incomplete fixtures keep working.
// Callbacks keep this resolver free of Win32 APIs so config_test drives the production logic directly:
//   readSettingsXml(&text) -> bool
//   readStockPresetXml(presetStem, &text) -> bool
//   readUserFxcfgXml(fileName, &text) -> bool
//   listUserFxcfg() -> std::vector<PresetFxcfgEntry>
template <typename ReadSettingsFn, typename ReadStockFn, typename ReadUserFn, typename ListUserFn>
inline bool resolveActivePresetMultipliers(ReadSettingsFn&& readSettingsXml,
                                           ReadStockFn&& readStockPresetXml,
                                           ReadUserFn&& readUserFxcfgXml,
                                           ListUserFn&& listUserFxcfg,
                                           float* hmdOut, float* ssaaOut,
                                           std::string* sourceOut) {
    if (hmdOut) *hmdOut = 0.0f;
    if (ssaaOut) *ssaaOut = 0.0f;
    if (sourceOut) sourceOut->clear();

    std::string settingsXml;
    std::string preset;
    const bool hasPreset = readSettingsXml(&settingsXml) && parseSettingsPresetName(settingsXml, &preset);

    std::string xml;
    std::string source;
    if (hasPreset) {
        if (_stricmp(preset.c_str(), "Custom") == 0) {
            const std::vector<PresetFxcfgEntry> files = listUserFxcfg();
            // Match src/d3d11/terrain_checkerboard_reader.h's active-preset selection rules:
            // only exact Custom.<major>.<minor>.fxcfg (or Custom.<major>.fxcfg / Custom.fxcfg)
            // shapes count; lookalikes like CustomOld.fxcfg or Custom.beta.fxcfg are never read
            // by Elite and must leave auto bias unset rather than apply an inactive preset's scale.
            const int pick = pickPresetFxcfg("Custom", files);
            if (pick < 0) return false;
            source = files[static_cast<size_t>(pick)].name;
            if (!readUserFxcfgXml(source, &xml) || xml.empty()) return false;
        } else {
            if (!isSafePresetStem(preset)) return false;
            if (readStockPresetXml(preset, &xml) && !xml.empty()) {
                source = "OptionDefaults\\" + preset + ".fxcfg";
            } else {
                const std::vector<PresetFxcfgEntry> files = listUserFxcfg();
                const int pick = pickPresetFxcfg(preset, files);
                if (pick < 0) return false;
                source = files[static_cast<size_t>(pick)].name;
                if (!readUserFxcfgXml(source, &xml) || xml.empty()) return false;
            }
        }
    } else {
        const std::vector<PresetFxcfgEntry> files = listUserFxcfg();
        int newest = -1;
        for (size_t i = 0; i < files.size(); ++i) {
            if (!endsWithFxcfg(files[i].name)) continue;
            if (newest < 0 || files[i].mtime > files[static_cast<size_t>(newest)].mtime) {
                newest = static_cast<int>(i);
            }
        }
        if (newest < 0) return false;
        source = files[static_cast<size_t>(newest)].name;
        if (!readUserFxcfgXml(source, &xml) || xml.empty()) return false;
    }

    if (sourceOut) *sourceOut = source;
    return parseFxcfgMultipliers(xml, hmdOut, ssaaOut);
}

struct TextureLodBiasResult {
    float bias = 0.0f;
    bool isAuto = false;
    bool autoMissing = false;
    float activeMult = 0.0f;
    char why[256] = {};
};

// Derives the sampler mip bias and explanatory log fragment from the configured
// biasKey ("auto" or a numeric string) and the multipliers read from .fxcfg.
inline TextureLodBiasResult resolveTextureLodBias(bool flatProfile, const std::string& biasKey,
                                                  float hmdMult, float ssaaMult,
                                                  const char* presetFile) {
    TextureLodBiasResult out;
    const char* fileLabel = (presetFile && presetFile[0]) ? presetFile : "its graphics preset";
    if (_stricmp(biasKey.c_str(), "auto") == 0) {
        const float active = flatProfile ? ssaaMult : hmdMult;
        if (active > 0.0f) {
            out.bias = std::log2(active);
            out.isAuto = true;
            out.activeMult = active;
            if (flatProfile) {
                std::snprintf(
                    out.why, sizeof(out.why),
                    "auto: Elite renders at %.3f of the output size "
                    "(SSAAMultiplier in %s; its HMDRenderTargetMultiplier is %.3f), and "
                    "log2 of that is the bias the mips want",
                    static_cast<double>(ssaaMult), fileLabel, static_cast<double>(hmdMult));
            } else {
                std::snprintf(
                    out.why, sizeof(out.why),
                    "auto: Elite renders at %.3f of the size the runtime recommends "
                    "(HMDRenderTargetMultiplier in %s; its SSAAMultiplier is %.3f), and "
                    "log2 of that is the bias the mips want",
                    static_cast<double>(hmdMult), fileLabel, static_cast<double>(ssaaMult));
            }
        } else {
            out.autoMissing = true;
        }
    } else {
        out.bias = static_cast<float>(std::atof(biasKey.c_str()));
        std::snprintf(out.why, sizeof(out.why), "advanced.texture_lod_bias = %s", biasKey.c_str());
    }
    if (!std::isfinite(out.bias)) out.bias = 0.0f;
    if (out.bias < -4.0f) out.bias = -4.0f;
    if (out.bias > 4.0f) out.bias = 4.0f;
    return out;
}

// Effective advanced.texture_lod_bias snapshot used by mode_watch.cpp's
// `kept until restart:` check on live VR <-> flat switches (#253).
// Labels contain no whitespace so tools/edup_log.py's KEPT_ITEM (\S+) parser
// matches them cleanly.
struct PatchedTextureLodBias {
    std::string label;
    float bias = 0.0f;
    bool resolved = false;
};

inline PatchedTextureLodBias effectiveTextureLodBias(bool flatProfile, const std::string& biasKey,
                                                     float hmdMult, float ssaaMult) {
    PatchedTextureLodBias out;
    const TextureLodBiasResult r = resolveTextureLodBias(flatProfile, biasKey, hmdMult, ssaaMult, nullptr);
    out.bias = r.bias;
    const bool isAuto = _stricmp(biasKey.c_str(), "auto") == 0;
    if (isAuto && r.autoMissing) {
        out.label = "auto";
        out.resolved = false;
        return out;
    }
    out.resolved = true;
    const double clean = std::fabs(static_cast<double>(r.bias)) < 0.0005 ? 0.0 : static_cast<double>(r.bias);
    char buf[32];
    if (isAuto) {
        std::snprintf(buf, sizeof(buf), "auto(%.2f)", clean);
        out.label = buf;
    } else {
        // Trim any surrounding whitespace so KEPT_ITEM (\S+) always parses the token.
        size_t s = 0;
        while (s < biasKey.size() && (biasKey[s] == ' ' || biasKey[s] == '\t')) ++s;
        size_t e = biasKey.size();
        while (e > s && (biasKey[e - 1] == ' ' || biasKey[e - 1] == '\t')) --e;
        std::string trimmed = biasKey.substr(s, e - s);
        if (trimmed.empty() || trimmed.find_first_of(" \t\r\n") != std::string::npos) {
            std::snprintf(buf, sizeof(buf), "%.2f", clean);
            out.label = buf;
        } else {
            out.label = std::move(trimmed);
        }
    }
    return out;
}

inline bool sameEffectiveTextureLodBias(const PatchedTextureLodBias& a, const PatchedTextureLodBias& b) {
    if (a.resolved && b.resolved) return std::fabs(a.bias - b.bias) < 0.005f;
    return a.resolved == b.resolved && a.label == b.label;
}

}  // namespace edvr
