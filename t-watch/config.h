#ifndef CONFIG_H
#define CONFIG_H

// Hardware defines for TTGO TWatch Library
#define LILYGO_WATCH_2019_WITH_TOUCH1         // T-Watch 2021 with PSRAM
#define LILYGO_WATCH_HAS_MOTOR     // Enable vibration motor
#define LILYGO_WATCH_HAS_BUZZER    // Enable buzzer (assume GPIO25)

// Custom pins (if not handled by library)
#define MOTOR_PIN 4                // Vibration motor
#define BUZZER_PIN 25              // Buzzer (adjust if different)

#endif