#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "valkyries";

#define BUZZER_GPIO      4
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_CHANNEL     LEDC_CHANNEL_0
#define LEDC_RESOLUTION  LEDC_TIMER_10_BIT   
#define DUTY_ON          512                 
#define NOTE_GAP_MS      15                

#define REST     0
#define NOTE_FS4 370
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_D5  587
#define NOTE_FS5 740
#define NOTE_A5  880
#define NOTE_B5  988

#define TEMPO_UNIT 110
#define S   (TEMPO_UNIT * 1)   
#define L   (TEMPO_UNIT * 3)   
#define H   (TEMPO_UNIT * 6)  

typedef struct {
    uint32_t freq_hz;  
    uint32_t dur_ms;
} note_t;

#define GALLOP(low, root, third) \
    { low, S }, { root, L }, { low, S }, { root, L }, { third, H }, { root, H }

static const note_t score[] = {
    GALLOP(NOTE_FS4, NOTE_B4,  NOTE_D5),    
    GALLOP(NOTE_B4,  NOTE_D5,  NOTE_FS5),   
    GALLOP(NOTE_D5,  NOTE_FS5, NOTE_A5),    
    GALLOP(NOTE_FS5, NOTE_A5,  NOTE_B5),    
    { REST, L },
    GALLOP(NOTE_FS4, NOTE_B4,  NOTE_D5),    
    { NOTE_B4, H * 2 },                     
};

#define SCORE_LEN (sizeof(score) / sizeof(score[0]))

static void buzzer_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = 1000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num   = BUZZER_GPIO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

static void buzzer_set_duty(uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

static void play_note(const note_t *n)
{
    if (n->freq_hz == REST) {
        buzzer_set_duty(0);
        vTaskDelay(pdMS_TO_TICKS(n->dur_ms));
        return;
    }

    ESP_LOGI(TAG, "%4lu Hz  %4lu ms", (unsigned long)n->freq_hz, (unsigned long)n->dur_ms);

    ESP_ERROR_CHECK(ledc_set_freq(LEDC_MODE, LEDC_TIMER, n->freq_hz));
    buzzer_set_duty(DUTY_ON);

    uint32_t sound_ms = n->dur_ms > NOTE_GAP_MS ? n->dur_ms - NOTE_GAP_MS : n->dur_ms;
    vTaskDelay(pdMS_TO_TICKS(sound_ms));

    buzzer_set_duty(0);
    vTaskDelay(pdMS_TO_TICKS(n->dur_ms - sound_ms));
}

void app_main(void)
{
    buzzer_init();
    ESP_LOGI(TAG, "Ride of the Valkyries — %u notes", (unsigned)SCORE_LEN);

    while (1) {
        for (size_t i = 0; i < SCORE_LEN; i++) {
            play_note(&score[i]);
        }
        ESP_LOGI(TAG, "--- кінець, повтор через 3 с ---");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}