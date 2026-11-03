#pragma once

/** The board's GT911 capacitive touch controller, on I2C. */
namespace touch {

bool begin();
/** Whether a finger is down; its position in screen coordinates if so. */
bool read(int &x, int &y);

}  // namespace touch
