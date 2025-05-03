#ifndef BUTTON_H
#define BUTTON_H

enum button_evt {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED,
};

typedef void (*button_evt_cb_t)(enum button_evt evt);
int button_init_dt(const struct gpio_dt_spec* spec, button_evt_cb_t cb);
#endif /* BUTTON */
