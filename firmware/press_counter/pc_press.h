// pc_press.h - press stroke detection on the opto-isolated 230 V sense input (GPIO34, LOW = voltage present).
// A stroke = one active period (merged over contact bounce shorter than PRESS_GAP_MS) that lasts at least minPulseMs.
// The edges are time-stamped in an interrupt so strokes are counted exactly even while the display or flash is busy.
#ifndef PC_PRESS_H
#define PC_PRESS_H

#include "pc_config.h"

namespace press {
typedef void (*StrokeCallback)(uint32_t durationMs);

void begin();
void poll();                          // call every loop
void onStroke(StrokeCallback cb);
void setCountEnabled(bool on);        // false = die test: strokes are timed but not counted

uint32_t count();                     // strokes since the last reset (session counter)
void resetCount();
uint32_t lastDurationMs();
bool everCounted();
uint32_t sinceLastStrokeMs();         // ms since the last counted stroke ended (0xFFFFFFFF = none yet)
bool active();                        // the sense input is active right now
uint32_t activeForMs();               // length of the running stroke (0 when idle)
bool inTrain();                       // a stroke ended less than PRESS_TRAIN_GAP_MS ago
uint32_t rejected();                  // active periods shorter than minPulseMs (noise)
}  // namespace press

#endif
