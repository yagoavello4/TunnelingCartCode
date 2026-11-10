#include <gpiod.h>
#include <cstdio>
#include <iostream>
#include <unistd.h>

int main() {
    gpiod_chip* chip = gpiod_chip_open_by_name("gpiochip4");
    if (!chip) {
        std::perror("Failed to open chip");
        return 1;
    }

    gpiod_line* direction = gpiod_chip_get_line(chip, 17);
    gpiod_line* power = gpiod_chip_get_line(chip, 10);
    if (!direction || !power) {
        std::perror("Failed to get GPIO line");
        gpiod_chip_close(chip);
        return 1;
    }

    // Acquire power OFF before requesting the direction line.
    if (gpiod_line_request_output(power, "linear_actuator", 0) < 0) {
        std::perror("Failed to request power output");
        gpiod_chip_close(chip);
        return 1;
    }
    if (gpiod_line_request_output(direction, "linear_actuator", 0) < 0) {
        std::perror("Failed to request direction output");
        gpiod_line_release(power);
        gpiod_chip_close(chip);
        return 1;
    }

    auto write = [](gpiod_line* line, int value) {
        if (gpiod_line_set_value(line, value) == 0) return true;
        std::perror("GPIO write failed");
        return false;
    };
    int result = 0;
    std::cout << "---- Linear Actuator Control Program ----\n"
                 "BCM GPIO17 = direction; GPIO10 = 12V power.\n"
                 "0 = exit, 1 = extend, 2 = retract, 3 = stop, 4 = power on\n";

    int input;
    while (true) {
        // Failed input must never repeat the previous motion command.
        if (!(std::cin >> input)) {
            if (!std::cin.eof()) {
                std::cerr << "Invalid input; stopping actuator\n";
                result = 1;
            }
            break;
        }
        if (input == 0) break;
        if (input == 1 || input == 2) {
            if (!write(power, 0)) { result = 1; break; }
            usleep(100000); // Remove power before changing direction.
            if (!write(direction, input == 2 ? 1 : 0)) { result = 1; break; }
            // Allow the direction relay to settle before restoring power.
            usleep(100000);
            if (!write(power, 1)) { result = 1; break; }
        } else if (input == 3 || input == 4) {
            if (!write(power, input == 4 ? 1 : 0)) { result = 1; break; }
        } else {
            std::cerr << "Unknown command; stopping actuator\n";
            if (!write(power, 0)) { result = 1; break; }
        }
    }

    // Do not switch direction during shutdown; power-off can itself fail.
    if (!write(power, 0)) result = 1;
    gpiod_line_release(direction);
    gpiod_line_release(power);
    gpiod_chip_close(chip);
    return result;
}
