#include "random_name.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef MICP_BUILD_SEED
#define MICP_BUILD_SEED_STR MICP_BUILD_SEED
#else
#define MICP_BUILD_SEED_STR "0"
#endif

#include <cstdint>
#include <random>
#include <mutex>
#include <string>

namespace {

// Deterministic per-build seed: every compile of the DLL produces its own
// class/package name space, and names stay reproducible within one build
// (useful when matching proxy.log lines across sessions). No word lists are
// used, so the generated identifiers carry no dictionary signature.
uint64_t BuildSeed() {
    uint64_t seed = 1469598103934665603ull; // FNV-1a offset basis
    for (const char* p = MICP_BUILD_SEED_STR; *p; ++p) {
        seed ^= static_cast<uint8_t>(*p);
        seed *= 1099511628211ull; // FNV-1a prime
    }
    return seed;
}

// Consonant/vowel alternation produces identifiers that look like ordinary
// obfuscator output (kavoteq, ziremnux) instead of raw garbage.
constexpr char kConsonants[] = "bcdfghjklmnpqrstvwxyz";
constexpr char kVowels[] = "aeiou";

std::string SeededName(std::mt19937_64& rng, int length) {
    static constexpr size_t kConsCount = sizeof(kConsonants) - 1;
    static constexpr size_t kVowelCount = sizeof(kVowels) - 1;
    std::uniform_int_distribution<size_t> cons(0, kConsCount - 1);
    std::uniform_int_distribution<size_t> vow(0, kVowelCount - 1);

    std::string out;
    out.reserve(static_cast<size_t>(length));
    bool wantVowel = (rng() & 1) != 0;
    for (int i = 0; i < length; ++i) {
        out.push_back(wantVowel ? kVowels[vow(rng)] : kConsonants[cons(rng)]);
        wantVowel = !wantVowel;
    }
    return out;
}

}

std::string GenerateRandomClassName(int minWords, int maxWords) {
    if (minWords < 1) minWords = 1;
    if (maxWords < minWords) maxWords = minWords;

    std::mt19937_64 rng(BuildSeed() ^
                        (static_cast<uint64_t>(GetCurrentProcessId()) << 32) ^
                        GetCurrentProcessId());

    std::uniform_int_distribution<int> nDist(minWords * 5, maxWords * 6);
    return SeededName(rng, nDist(rng));
}

std::string MakeInternalName(const std::string& internalPackage,
                             const std::string& simpleName) {
    if (internalPackage.empty()) return simpleName;
    std::string out = internalPackage;
    if (out.back() != '/') out.push_back('/');
    out += simpleName;
    return out;
}

namespace {

std::string GenerateRandomPackageName() {
    std::mt19937_64 rng(BuildSeed() ^ 0x9E3779B97F4A7C15ull);

    std::uniform_int_distribution<int> depthDist(1, 3);
    std::uniform_int_distribution<int> lenDist(4, 9);

    int depth = depthDist(rng);
    std::string out;
    for (int i = 0; i < depth; ++i) {
        if (i > 0) out += '/';
        out += SeededName(rng, lenDist(rng));
    }
    return out;
}

std::string g_trampolinePackage;
std::once_flag g_packageInitFlag;

}

const std::string& GetTrampolinePackage() {
    std::call_once(g_packageInitFlag, [] {
        g_trampolinePackage = GenerateRandomPackageName();
    });
    return g_trampolinePackage;
}
