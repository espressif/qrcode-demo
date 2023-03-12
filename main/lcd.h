#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void lcd_init(void);
void lcd_fill(int r, int g, int b);
void board_led_set(int on);
void lcd_draw_grayscale(uint8_t* buf, int w, int h);

#ifdef __cplusplus
}
#endif
