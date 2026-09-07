#pragma once

#include <windows.h>

enum UiLanguage { LANGUAGE_KOREAN = 0, LANGUAGE_ENGLISH, LANGUAGE_COUNT };

// translations.tsv is loaded next to the executable. The first column is the
// Korean source text and the second is English. Escapes and printf placeholders
// let UI copy be updated without recompiling the game.
void LoadTranslations();
void SetUiLanguage(int language);
int UiLanguage();
const wchar_t* LocalizeText(const wchar_t* source);

