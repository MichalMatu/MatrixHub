#pragma once

typedef int gpio_num_t;

inline constexpr gpio_num_t GPIO_NUM_0 = 0;

extern "C" int gpio_get_level(gpio_num_t gpio_num);
