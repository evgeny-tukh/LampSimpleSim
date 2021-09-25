#pragma once
#include <cstdint>
#include "Windows.h"

bool editText (HWND parent, HINSTANCE instance, char *caption, char *prompt, char *buffer, size_t size, char *initValue);
bool editNumber (HWND parent, HINSTANCE instance, char *caption, char *prompt, uint32_t *value, uint32_t initValue);
