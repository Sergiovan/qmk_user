LED_MATRIX_EFFECT(sgv_custom_led)

#ifdef LED_MATRIX_CUSTOM_EFFECT_IMPLS

static bool sgv_custom_led(effect_params_t* params);

#    include "sgv_custom_led.c"

#endif // LED_MATRIX_CUSTOM_EFFECT_IMPLS
