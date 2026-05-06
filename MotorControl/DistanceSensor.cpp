#include <unistd.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <gpiod.h>

/* definitions */
const char* GPIO_CHIP = "gpiochip0";

const int TRIG_PIN = 1;
const int ECHO_PIN = 2;

/* function prototypes */
double readDistanceCM(gpiod_line* trig, gpiod_line* echo);
void setupSensor(gpiod_chip*& chip, gpiod_line*& trig, gpiod_line*& echo);
void cleanupSensor(gpiod_chip* chip, gpiod_line* trig, gpiod_line* echo);

/* main */
int main() {
    gpiod_chip* chip;
    gpiod_line* trig;
    gpiod_line* echo;

    setupSensor(chip, trig, echo);

    std::cout << "HC-SR04 Distance Sensor Started\n";
    std::cout << "TRIG GPIO: " << TRIG_PIN << "\n";
    std::cout << "ECHO GPIO: " << ECHO_PIN << "\n";
    std::cout << "Press CTRL+C to stop\n\n";

    while (true) {
        double distance = readDistanceCM(trig, echo);

        if (distance < 0) {
            std::cout << "Distance read failed\n";
        } else {
            std::cout << "Distance: " << distance << " cm\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    cleanupSensor(chip, trig, echo);
    return 0;
}

/* setup GPIO sensor pins */
void setupSensor(gpiod_chip*& chip, gpiod_line*& trig, gpiod_line*& echo) {
    chip = gpiod_chip_open_by_name(GPIO_CHIP);

    if (!chip) {
        std::cerr << "Error: Could not open GPIO chip\n";
        exit(1);
    }

    trig = gpiod_chip_get_line(chip, TRIG_PIN);
    echo = gpiod_chip_get_line(chip, ECHO_PIN);

    if (!trig || !echo) {
        std::cerr << "Error: Could not get GPIO lines\n";
        gpiod_chip_close(chip);
        exit(1);
    }

    if (gpiod_line_request_output(trig, "hc-sr04-trig", 0) < 0) {
        std::cerr << "Error: Could not set TRIG as output\n";
        gpiod_chip_close(chip);
        exit(1);
    }

    if (gpiod_line_request_input(echo, "hc-sr04-echo") < 0) {
        std::cerr << "Error: Could not set ECHO as input\n";
        gpiod_line_release(trig);
        gpiod_chip_close(chip);
        exit(1);
    }
}

/* read distance in centimeters */
double readDistanceCM(gpiod_line* trig, gpiod_line* echo) {
    // make sure trigger starts LOW
    gpiod_line_set_value(trig, 0);
    std::this_thread::sleep_for(std::chrono::microseconds(2));

    // send 10 microsecond pulse to trigger
    gpiod_line_set_value(trig, 1);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    gpiod_line_set_value(trig, 0);

    // wait for echo to go HIGH
    auto timeoutStart = std::chrono::steady_clock::now();

    while (gpiod_line_get_value(echo) == 0) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - timeoutStart).count();

        if (elapsed > 0.1) {
            return -1;
        }
    }

    auto echoStart = std::chrono::steady_clock::now();

    // wait for echo to go LOW
    timeoutStart = std::chrono::steady_clock::now();

    while (gpiod_line_get_value(echo) == 1) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - timeoutStart).count();

        if (elapsed > 0.1) {
            return -1;
        }
    }

    auto echoEnd = std::chrono::steady_clock::now();

    double durationMicroseconds =
        std::chrono::duration<double, std::micro>(echoEnd - echoStart).count();

    // speed of sound = 0.0343 cm per microsecond
    // divide by 2 because sound goes to object and back
    double distanceCM = durationMicroseconds * 0.0343 / 2.0;

    return distanceCM;
}

/* cleanup GPIO */
void cleanupSensor(gpiod_chip* chip, gpiod_line* trig, gpiod_line* echo) {
    gpiod_line_release(trig);
    gpiod_line_release(echo);
    gpiod_chip_close(chip);
}
