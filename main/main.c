/*
 * "Сейф" - введення пін-коду енкодером (ESP32-S3, ESP-IDF)
 *
 * Підключення:
 *   Енкодер KY-040: CLK -> GPIO4, DT -> GPIO5, SW -> GPIO6, + -> 3V3, GND -> GND
 *   Бузер:          GPIO7 -> R1 1k -> база Q1 (BC547), колектор -> BZ1 -> +5V, D1 паралельно BZ1
 *   Серво SG90:     PWM -> GPIO14, + -> 5V, GND -> GND, C1 470 мкФ між 5V і GND
 *
 * Як вводити код (приклад для 2-0-2-6):
 *   CW  x3  -> 0,1,2          (перший тік = 0, далі +1)
 *   CCW x1  -> підтвердили 2, нова цифра = 0
 *   CW  x3  -> підтвердили 0, нова цифра 0,1,2
 *   CCW x7  -> підтвердили 2, нова цифра 0..6
 *   CW  x1  -> підтвердили 6 -> перевірка коду
 *   Кнопка  -> скидання (це теж спроба). У відкритому стані кнопка закриває замок.
 */

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "encoder.h"
#include "buzzer.h"
#include "servo.h"
#include "safe.h"

/* ---------- Піни ---------- */
#define ENC_CLK_PIN         GPIO_NUM_4
#define ENC_DT_PIN          GPIO_NUM_5
#define ENC_SW_PIN          GPIO_NUM_6
#define BUZZER_PIN          GPIO_NUM_7
#define SERVO_PIN           GPIO_NUM_14

/* ---------- Налаштування сейфа ---------- */
static const uint8_t SAFE_CODE[] = { 2, 0, 2, 6 };
#define SAFE_MAX_ATTEMPTS   3
#define SHOW_DIGITS         1       /* 0 - показувати '*' замість цифр */

#define SERVO_LOCKED_DEG    10
#define SERVO_OPEN_DEG      100

#define ALARM_REPEATS       5
#define LOOP_PERIOD_MS      10

static const char *TAG = "SAFE";

/* ---------- Мелодії ---------- */
static const buzzer_note_t MELODY_START[] = {
    { 1047, 60 }, { 1568, 80 },
};
static const buzzer_note_t MELODY_OK[] = {
    { 523, 120 }, { 659, 120 }, { 784, 120 }, { 1047, 300 },
};
static const buzzer_note_t MELODY_WRONG[] = {
    { 330, 200 }, { 220, 350 },
};
static const buzzer_note_t MELODY_RESET[] = {
    { 440, 80 }, { 0, 40 }, { 440, 80 },
};
static const buzzer_note_t MELODY_CLOSED[] = {
    { 784, 100 }, { 523, 150 },
};
static const buzzer_note_t MELODY_ALARM[] = {    /* сирена */
    { 880, 250 }, { 660, 250 }, { 880, 250 }, { 660, 250 },
};

#define TICK_FREQ_HZ        2000
#define TICK_MS             20

/* ---------- Консоль ---------- */
static char digit_char(uint8_t d)
{
    return SHOW_DIGITS ? (char)('0' + d) : '*';
}

static void print_prompt(const safe_t *s)
{
    printf("\n[Attempt %u/%u] Code: ", safe_attempt_number(s), s->max_attempts);
    fflush(stdout);
}

/* ---------- Реакції на події ---------- */
static void on_locked_out(void)
{
    encoder_disable();
    ESP_LOGE(TAG, "No attempts left. ALARM! Device locked until reboot.");

    for (int i = 0; i < ALARM_REPEATS; i++) {
        BUZZER_PLAY(MELODY_ALARM);
    }

    ESP_LOGE(TAG, "LOCKED. Press RESET (EN) on the board to restart.");
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void handle_event(safe_t *s, safe_event_t ev)
{
    switch (ev) {
    case SAFE_EV_DIGIT_STARTED:
        /* пробіл між цифрами; для першої цифри - без нього */
        printf("%s%c", s->entered_count ? " " : "", digit_char(s->current));
        fflush(stdout);
        buzzer_tone(TICK_FREQ_HZ, TICK_MS);
        break;

    case SAFE_EV_DIGIT_CHANGED:
        printf("\b%c", digit_char(s->current));  /* затираємо попередню цифру */
        fflush(stdout);
        buzzer_tone(TICK_FREQ_HZ, TICK_MS);
        break;

    case SAFE_EV_CODE_OK:
        printf("  -> OK\n");
        ESP_LOGI(TAG, "ACCESS GRANTED. Lock opened. Press button to close.");
        servo_set_angle(SERVO_OPEN_DEG);
        BUZZER_PLAY(MELODY_OK);
        break;

    case SAFE_EV_CODE_WRONG:
        printf("  -> WRONG\n");
        ESP_LOGW(TAG, "Wrong code. Attempts left: %u", safe_attempts_left(s));
        BUZZER_PLAY(MELODY_WRONG);
        print_prompt(s);
        break;

    case SAFE_EV_RESET:
        printf("  -> RESET\n");
        ESP_LOGW(TAG, "Input reset. Attempts left: %u", safe_attempts_left(s));
        BUZZER_PLAY(MELODY_RESET);
        print_prompt(s);
        break;

    case SAFE_EV_LOCKOUT:
        printf("  -> FAIL\n");
        on_locked_out();            /* не повертається */
        break;

    case SAFE_EV_CLOSED:
        ESP_LOGI(TAG, "Lock closed.");
        servo_set_angle(SERVO_LOCKED_DEG);
        BUZZER_PLAY(MELODY_CLOSED);
        print_prompt(s);
        break;

    case SAFE_EV_NONE:
    default:
        return;
    }

    /* Поки грала мелодія, енкодер міг накрутити зайве - відкидаємо,
     * але не після тіків, інакше загубимо швидке обертання. */
    if (ev != SAFE_EV_DIGIT_STARTED && ev != SAFE_EV_DIGIT_CHANGED) {
        encoder_discard();
    }
}

void app_main(void)
{
    static safe_t safe;

    if (!safe_init(&safe, SAFE_CODE, sizeof(SAFE_CODE), SAFE_MAX_ATTEMPTS)) {
        ESP_LOGE(TAG, "Invalid safe config (code length 1..%d, digits 0..9)", SAFE_MAX_DIGITS);
        return;
    }

    encoder_init(ENC_CLK_PIN, ENC_DT_PIN, ENC_SW_PIN);
    buzzer_init(BUZZER_PIN);
    servo_init(SERVO_PIN);

    servo_set_angle(SERVO_LOCKED_DEG);
    BUZZER_PLAY(MELODY_START);
    encoder_discard();

    ESP_LOGI(TAG, "=== Safe ready: %u digits, %u attempts ===", safe.code_len, safe.max_attempts);
    ESP_LOGI(TAG, "Rotate: +1 | change direction: next digit | button: reset");
    print_prompt(&safe);

    while (1) {
        int detents = encoder_take_detents();

        while (detents != 0) {
            int dir = (detents > 0) ? +1 : -1;
            detents -= dir;

            safe_event_t ev = safe_on_tick(&safe, dir);
            handle_event(&safe, ev);

            /* Код перевірено: решту тіків цієї пачки не застосовуємо */
            if (ev != SAFE_EV_DIGIT_STARTED && ev != SAFE_EV_DIGIT_CHANGED) {
                break;
            }
        }

        if (encoder_button_pressed()) {
            handle_event(&safe, safe_on_button(&safe));
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}
