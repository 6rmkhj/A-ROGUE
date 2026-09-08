#pragma once

#include <windows.h>

enum UiLanguage { LANGUAGE_KOREAN = 0, LANGUAGE_ENGLISH, LANGUAGE_COUNT };

// translations.tsv is loaded next to the executable. The first column is the
// Korean source text and the second is English. Escapes and printf placeholders
// let UI copy be updated without recompiling the game.
void LoadTranslations();
// Zero when translations.tsv was missing or held no usable rows. English is
// then unavailable: picking it would silently show Korean, so callers disable
// the option and say why instead.
int TranslationsLoaded();
void SetUiLanguage(int language);
int UiLanguage();
const wchar_t* LocalizeText(const wchar_t* source);

