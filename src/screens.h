#pragma once

#include "ui.h"
#include "weather.h"

enum Page { NOW, WEEK, PAGE_COUNT };

/** Page 1: the temperature large, and what the rain will do in the next two
 * hours, as a sentence and as bars. */
void drawNow();
/** Page 2: the next seven days, one row each. */
void drawWeek();
