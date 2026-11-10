#include <gpiod.h>
#include <cstdio>
#include <iostream>
#include <unistd.h>

#define GPIO_CHIP "gpiochip4"
#define GPIO_LINE 17 // BCM GPIO17 (pin 11)

int main() {
    gpiod_chip* chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) {
        std::perror("Open chip failed");
        return 1;
    }
    gpiod_line* line = gpiod_chip_get_line(chip, GPIO_LINE);
    if (!line) {
        std::perror("Get line failed");
        gpiod_chip_close(chip);
        return 1;
    }
    if (gpiod_line_request_output(line, "gpio-out", 0) < 0) {
        std::perror("Request line as output failed");
        gpiod_chip_close(chip);
        return 1;
    }

    int result = 0;
    std::cout << "Toggling GPIO " << GPIO_LINE << "...\n";
    for (int i = 0; i < 10; ++i) {
        if (gpiod_line_set_value(line, 1) < 0) {
            std::perror("Set GPIO HIGH failed");
            result = 1;
            break;
        }
        sleep(1);
        if (gpiod_line_set_value(line, 0) < 0) {
            std::perror("Set GPIO LOW failed");
            result = 1;
            break;
        }
        sleep(1);
    }
    // Best-effort LOW even when a previous write failed.
    if (gpiod_line_set_value(line, 0) < 0) {
        std::perror("Shutdown GPIO LOW failed");
        result = 1;
    }
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return result;
}
