#ifndef LED_STRIP_H_
#define LED_STRIP_H_

// 滚动方向枚举
typedef enum { SCROLL_LEFT, SCROLL_RIGHT } scroll_direction_t;

// 初始化LED条
int led_strip_init(void);

// 开始滚动显示文本
void led_strip_scroll_text(const char* str, scroll_direction_t direction);

// 更新滚动显示（需要在主循环中定期调用）
void led_strip_scroll_update(void);
void led_strip_set_char_spacing(uint8_t spacing);
// 停止滚动显示
void led_strip_stop_scroll(void);

// 保持向后兼容的数字显示功能
void led_strip_display_number(uint8_t number);

#endif // LED_STRIP_H_