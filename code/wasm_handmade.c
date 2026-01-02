#include "wasm_handmade.h"

float audio_buffer[128 * 2];

void setup() {}

void update_and_render() {}

float *output_audio(int sample_count) { return audio_buffer; }

void mouse_move(int x, int y) {}
void mouse_button_press(int button) {}
void mouse_button_release(int button) {}

void scroll_wheel(int x, int y) {}

void controller_stick(int controller, int x, int y) {}
void controller_button_press(int controller, int button) {}
void controller_button_release(int controller, int button) {}
