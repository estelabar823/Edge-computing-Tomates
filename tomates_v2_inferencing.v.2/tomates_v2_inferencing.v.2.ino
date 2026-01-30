// ESP32 + Camera + Edge Impulse + WS2812 (FastLED)
// Ajusta includes y defines según tu proyecto

#include "tomates_v2_inferencing.h"                 // Tu header con run_classifier y constantes
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

#define LED_PIN 13 
#define LED_COUNT 1

// Camera model selection (ESP-EYE / Timer Camera F)
#define CAMERA_MODEL_ESP_EYE

#if defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   15
#define XCLK_GPIO_NUM    27
#define SIOD_GPIO_NUM    25
#define SIOC_GPIO_NUM    23
#define Y9_GPIO_NUM      19
#define Y8_GPIO_NUM      36
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM       5
#define Y4_GPIO_NUM      34
#define Y3_GPIO_NUM      35
#define Y2_GPIO_NUM      32
#define VSYNC_GPIO_NUM   22
#define HREF_GPIO_NUM    26
#define PCLK_GPIO_NUM    21
#else
#error "Define your camera model!"
#endif

// Camera buffer sizes
/*#define EI_CAMERA_RAW_FRAME_BUFFER_COLS  320 //Inicial
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS  240*/ //Inicial
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 160 //Nuevo
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 120 //Nuevo
#define EI_CAMERA_FRAME_BYTE_SIZE        3

static bool debug_nn = false;
static bool is_initialised = false;

// Buffer estático para evitar malloc repetido
static uint8_t *snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE); 

static camera_config_t camera_config = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sscb_sda   = SIOD_GPIO_NUM,
    .pin_sscb_scl   = SIOC_GPIO_NUM,

    .pin_d7         = Y9_GPIO_NUM,
    .pin_d6         = Y8_GPIO_NUM,
    .pin_d5         = Y7_GPIO_NUM,
    .pin_d4         = Y6_GPIO_NUM,
    .pin_d3         = Y5_GPIO_NUM,
    .pin_d2         = Y4_GPIO_NUM,
    .pin_d1         = Y3_GPIO_NUM,
    .pin_d0         = Y2_GPIO_NUM,
    .pin_vsync      = VSYNC_GPIO_NUM,
    .pin_href       = HREF_GPIO_NUM,
    .pin_pclk       = PCLK_GPIO_NUM,

    .xclk_freq_hz   = 20000000,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,

    .pixel_format   = PIXFORMAT_RGB565,   
    .frame_size     = FRAMESIZE_QQVGA,  
    .jpeg_quality   = 12,
    .fb_count       = 1,
    .fb_location    = CAMERA_FB_IN_DRAM,  
    .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
};


// Prototipos
bool ei_camera_init(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

void setup() { 
  Serial.begin(115200); 
  pinMode(LED_PIN, OUTPUT); 
  pinMode(4, OUTPUT); 
  delay(3000); // 3 segundos para que te dé tiempo a abrir el monitor

  if (!ei_camera_init()) {
    Serial.println("Fallo en ei_camera_init()");
  } 
  else {
    Serial.println("Cámara inicializada OK");
  }

}


void loop() {
    //Nuevo
    Serial.println("Loop vivo");
    delay(500);

    // Preparar señal para Edge Impulse
    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    // Capturar imagen y convertir a RGB en snapshot_buf
    if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
        Serial.println("Error capturando imagen");
        delay(200);
        return;
    }

    // Ejecutar clasificador
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);

    if (err != EI_IMPULSE_OK) {
        Serial.printf("Error al clasificar: %d\n", err);
        delay(200);
        return;
    }

    /*
    //Inicial
    // Leer confidencias
    float maduro_conf = 0.0f;
    float no_maduro_conf = 0.0f;

    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("%s: %.5f\n", ei_classifier_inferencing_categories[i], result.classification[i].value);
        if (strcmp(ei_classifier_inferencing_categories[i], "maduro") == 0) {
            maduro_conf = result.classification[i].value;
        } else if (strcmp(ei_classifier_inferencing_categories[i], "no_maduro") == 0) {
            no_maduro_conf = result.classification[i].value;
        }
    }
    */

    /*
    //Nuevo v.4
    float maduro_conf = 0.0f;
    float no_maduro_conf = 0.0f;

    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      const char* label = ei_classifier_inferencing_categories[i];
      float value = result.classification[i].value;

      Serial.printf("%s: %.5f\n", label, value);

      if (strstr(label, "maduro") != NULL) {
        maduro_conf = value;
      }
      if (strstr(label, "no") != NULL || strstr(label, "verde") != NULL) {
        no_maduro_conf = value;
      }
    }
    */

    //Nuevo v.5
    float maduro_conf = 0.0f;
    float no_maduro_conf = 0.0f;
    
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    const char* label = ei_classifier_inferencing_categories[i];
    float value = result.classification[i].value;

    Serial.printf("%s: %.5f\n", label, value);

    if (strcmp(label, "Tomates Maduros") == 0) {
        maduro_conf = value;
    } else if (strcmp(label, "Tomates Verdes") == 0) {
        no_maduro_conf = value;
    }
}

Serial.printf("maduro_conf final = %.5f\n", maduro_conf);
Serial.printf("no_maduro_conf final = %.5f\n", no_maduro_conf);

    //Comprobación
    Serial.printf("maduro_conf final = %.5f\n", maduro_conf);
    Serial.printf("no_maduro_conf final = %.5f\n", no_maduro_conf);

    // Umbral y control LED
    const float UMBRAL = 0.70f;

    /*
    //Prueba
    float maduro_conf = 0.0f;
    float no_maduro_conf = 0.8f;
    */

    /*
    //NeoPixelBus
    if (maduro_conf >= UMBRAL) {
        //leds[0] = CRGB::Red; //FastLED
        strip.SetPixelColor(0, RgbColor(255, 0, 0)); //NeoPixelBus
    } else if (no_maduro_conf >= UMBRAL) {
        //leds[0] = CRGB::Green; //FastLED
        strip.SetPixelColor(0, RgbColor(0, 255, 0)); //NeoPixelBus
    } else {
        //leds[0] = CRGB::Blue; //FastLED
        strip.SetPixelColor(0, RgbColor(0, 0, 255)); //NeoPixelBus
    }
    */

    /*
    //LED simple
    if (maduro_conf >= UMBRAL) { 
      digitalWrite(LED_PIN, HIGH); // LED encendido → tomate maduro 
      //digitalWrite(13, HIGH); // LED encendido → tomate maduro //Estela
    } 
    else { 
      digitalWrite(LED_PIN, LOW); // LED apagado → no maduro o incierto 
      //digitalWrite(13, LOW); // LED apagado → no maduro o incierto //Estela
    */

    //LED semáforo //Estela
    if (maduro_conf >= UMBRAL) {
        digitalWrite(LED_PIN, LOW);
        digitalWrite(4, HIGH);
    } else if (no_maduro_conf >= UMBRAL) {
        digitalWrite(4, LOW);
        digitalWrite(LED_PIN, HIGH);
    } else {
        digitalWrite(LED_PIN, LOW);
        digitalWrite(4, LOW);
    }

    // Mostrar LED (FastLED maneja la temporización)
    //FastLED.show(); //Inicial //FastLED
    //strip.Show(); //NeoPixelBus

    // Pequeña pausa para evitar saturar la cámara/CPU
    delay(200);
}

/* ========================= FUNCIONES DE CÁMARA ============================ */

bool ei_camera_init(void) {
    if (is_initialised) return true;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        if (s->id.PID == OV3660_PID) {
            s->set_vflip(s, 1);
            s->set_brightness(s, 1);
            s->set_saturation(s, 0);
        }
        s->set_vflip(s, 1);
        s->set_hmirror(s, 1);
        s->set_awb_gain(s, 1);
    }

    is_initialised = true;
    return true;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    if (!is_initialised) return false;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("fb null");
        return false;
    }

    // Convertir a RGB888 en snapshot_buf (ya reservado)
    //bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf); //Inicial
    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_RGB565, snapshot_buf); //Nuevo

    esp_camera_fb_return(fb);

    if (!converted) {
        Serial.println("Conversión a RGB fallida");
        return false;
    }

    // Si el tamaño de salida difiere, recortar/interpolar
    if (img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS || img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS) {
        ei::image::processing::crop_and_interpolate_rgb888(
            snapshot_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height
        );
    } else {
        // Si coinciden, copiar directamente al buffer de salida
        size_t bytes = EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE;
        memcpy(out_buf, snapshot_buf, bytes);
    }

    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    // offset y length están en píxeles para la entrada del clasificador
    size_t pixel_ix = offset * 3;
    size_t out_ix = 0;
    for (size_t i = 0; i < length; i++) {
        // Construimos un valor RGB empaquetado en un float como espera Edge Impulse
        uint32_t packed = (snapshot_buf[pixel_ix + 2] << 16) | (snapshot_buf[pixel_ix + 1] << 8) | (snapshot_buf[pixel_ix]);
        out_ptr[out_ix++] = (float)packed;
        pixel_ix += 3;
    }
    return 0;
}
