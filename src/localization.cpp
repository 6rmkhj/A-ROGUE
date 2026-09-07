#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "localization.h"

struct TranslationEntry {
    std::wstring source;
    std::wstring english;
    int formatted;
};

static std::vector<TranslationEntry> gTranslations;
static int gUiLanguage = LANGUAGE_KOREAN;
static wchar_t gLocalizedBuffers[8][2048];
static int gLocalizedBufferIndex;

static std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return std::wstring();
    int count = MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), 0, 0);
    if (count <= 0) return std::wstring();
    std::wstring result((size_t)count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), &result[0], count);
    if (!result.empty() && result[0] == 0xFEFF) result.erase(result.begin());
    return result;
}

static std::wstring Unescape(const std::wstring& value) {
    std::wstring result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != L'\\' || i + 1 >= value.size()) { result += value[i]; continue; }
        wchar_t next = value[++i];
        if (next == L'n') result += L'\n';
        else if (next == L't') result += L'\t';
        else if (next == L'r') result += L'\r';
        else if (next == L'\\') result += L'\\';
        else { result += L'\\'; result += next; }
    }
    return result;
}

static int IsFormatStart(const std::wstring& value, size_t at, size_t* end) {
    if (at >= value.size() || value[at] != L'%') return 0;
    size_t i = at + 1;
    if (i < value.size() && value[i] == L'%') { *end = i + 1; return 2; }
    while (i < value.size() && (value[i] == L'+' || value[i] == L'-' || value[i] == L'0'
        || (value[i] >= L'1' && value[i] <= L'9') || value[i] == L'.')) ++i;
    if (i < value.size() && (value[i] == L'd' || value[i] == L'i' || value[i] == L'u'
        || value[i] == L's' || value[i] == L'c')) { *end = i + 1; return 1; }
    return 0;
}

static std::wstring NextLiteral(const std::wstring& format, size_t at) {
    std::wstring literal;
    for (size_t i = at; i < format.size();) {
        size_t end = 0;
        int kind = IsFormatStart(format, i, &end);
        if (kind == 1) break;
        if (kind == 2) { literal += L'%'; i = end; }
        else { literal += format[i]; ++i; }
    }
    return literal;
}

static int MatchFormatted(const std::wstring& format, const wchar_t* shown,
    std::vector<std::wstring>* captures) {
    std::wstring value(shown ? shown : L"");
    size_t fi = 0, vi = 0;
    while (fi < format.size()) {
        size_t end = 0;
        int kind = IsFormatStart(format, fi, &end);
        if (kind == 2) {
            if (vi >= value.size() || value[vi] != L'%') return 0;
            fi = end; ++vi; continue;
        }
        if (kind == 1) {
            // Adjacent placeholders (for example "%s%s") have no delimiter.
            // Give the first one an empty capture and let the final placeholder
            // consume through the next literal; their combined text is preserved.
            size_t adjacentEnd = 0;
            if (IsFormatStart(format, end, &adjacentEnd) == 1) {
                captures->push_back(std::wstring());
                fi = end; continue;
            }
            std::wstring next = NextLiteral(format, end);
            size_t stop = next.empty() ? value.size() : value.find(next, vi);
            if (stop == std::wstring::npos) return 0;
            captures->push_back(value.substr(vi, stop - vi));
            vi = stop; fi = end; continue;
        }
        if (vi >= value.size() || format[fi] != value[vi]) return 0;
        ++fi; ++vi;
    }
    return vi == value.size();
}

static std::wstring ExpandFormatted(const std::wstring& format,
    const std::vector<std::wstring>& captures) {
    std::wstring result;
    size_t capture = 0;
    for (size_t i = 0; i < format.size();) {
        size_t end = 0;
        int kind = IsFormatStart(format, i, &end);
        if (kind == 2) { result += L'%'; i = end; continue; }
        if (kind == 1) {
            if (capture < captures.size()) result += LocalizeText(captures[capture].c_str());
            ++capture; i = end; continue;
        }
        result += format[i++];
    }
    return result;
}

void LoadTranslations() {
    gTranslations.clear();
    wchar_t path[MAX_PATH];
    DWORD length = GetModuleFileNameW(0, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) lstrcpyW(slash + 1, L"translations.tsv");

    FILE* file = 0;
#ifdef _MSC_VER
    _wfopen_s(&file, path, L"rb");
    if (!file) _wfopen_s(&file, L"translations.tsv", L"rb");
#else
    file = _wfopen(path, L"rb");
    if (!file) file = _wfopen(L"translations.tsv", L"rb");
#endif
    if (!file) return;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::string bytes(size > 0 ? (size_t)size : 0, '\0');
    if (size > 0) fread(&bytes[0], 1, (size_t)size, file);
    fclose(file);

    size_t start = 0;
    while (start <= bytes.size()) {
        size_t finish = bytes.find('\n', start);
        if (finish == std::string::npos) finish = bytes.size();
        std::string line = bytes.substr(start, finish - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        start = finish + 1;
        if (line.empty() || line[0] == '#') continue;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::wstring source = Unescape(Utf8ToWide(line.substr(0, tab)));
        std::wstring english = Unescape(Utf8ToWide(line.substr(tab + 1)));
        if (source.empty()) continue;
        TranslationEntry entry = {source, english, source.find(L'%') != std::wstring::npos};
        gTranslations.push_back(entry);
    }
}

void SetUiLanguage(int language) {
    if (language >= 0 && language < LANGUAGE_COUNT) gUiLanguage = language;
}

int UiLanguage() { return gUiLanguage; }

const wchar_t* LocalizeText(const wchar_t* source) {
    if (!source || gUiLanguage != LANGUAGE_ENGLISH || gTranslations.empty()) return source;
    for (size_t i = 0; i < gTranslations.size(); ++i) {
        const TranslationEntry& entry = gTranslations[i];
        if (!entry.formatted && entry.source == source) return entry.english.c_str();
    }
    for (size_t i = 0; i < gTranslations.size(); ++i) {
        const TranslationEntry& entry = gTranslations[i];
        if (!entry.formatted) continue;
        std::vector<std::wstring> captures;
        if (!MatchFormatted(entry.source, source, &captures)) continue;
        gLocalizedBufferIndex = (gLocalizedBufferIndex + 1) % 8;
        std::wstring expanded = ExpandFormatted(entry.english, captures);
        lstrcpynW(gLocalizedBuffers[gLocalizedBufferIndex], expanded.c_str(), 2048);
        return gLocalizedBuffers[gLocalizedBufferIndex];
    }
    return source;
}
