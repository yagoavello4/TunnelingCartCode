#include <iostream>
#include <chrono>
#include <thread>
#include <gpiod.h>

#define TRIG_PIN 1
#define ECHO_PIN 2

#define CHIP_NAME "gpiochip0"

using namespace std;
using namespace chrono;

int main() {
    gpiod_chip *chip;
    gpiod_line *trig;
    gpiod_line *echo;

    chip = gpiod_chip_open_by_name(CHIP_NAME);

    if (!chip) {
        cerr << "Error: Could not open GPIO chip" << endl;
        return 1;
    }

    trig = gpiod_chip_get_line(chip, TRIG_PIN);
    echo = gpiod_chip_get_line(chip, ECHO_PIN);

    if (!trig || !echo) {
        cerr << "Error: Could not get GPIO lines" << endl;
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(trig, "hc-sr04-trig", 0) < 0) {
        cerr << "Error: Could not set TRIG as output" << endl;
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_input(echo, "hc-sr04-echo") < 0) {
        cerr << "Error: Could not set ECHO as input" << endl;
        gpiod_chip_close(chip);
        return 1;
    }

    cout << "HC-SR04 distance sensor started..." << endl;

    while (true) {
        // Make sure trigger starts LOW
        gpiod_line_set_value(trig, 0);
        this_thread::sleep_for(microseconds(2));

        // Send 10 microsecond HIGH pulse to trigger
        gpiod_line_set_value(trig, 1);
        this_thread::sleep_for(microseconds(10));
        gpiod_line_set_value(trig, 0);

        // Wait for echo to go HIGH
        auto timeoutStart = high_resolution_clock::now();
        while (gpiod_line_get_value(echo) == 0) {
            if (duration_cast<milliseconds>(high_resolution_clock::now() - timeoutStart).count() > 100) {
                cout << "Timeout waiting for echo HIGH" << endl;
                continue;
            }
        }

        auto startTime = high_resolution_clock::now();

        // Wait for echo to go LOW
        timeoutStart = high_resolution_clock::now();
        while (gpiod_line_get_value(echo) == 1) {
            if (duration_cast<milliseconds>(high_resolution_clock::now() - timeoutStart).count() > 100) {
                cout << "Timeout waiting for echo LOW" << endl;
                break;
            }
        }

        auto endTime = high_resolution_clock::now();

        auto duration = duration_cast<microseconds>(endTime - startTime).count();

        // Speed of sound: 343 m/s = 0.0343 cm/us
        // Divide by 2 because sound travels to object and back
        double distanceCm = duration * 0.0343 / 2.0;

        cout << "Distance: " << distanceCm << " cm" << endl;

        this_thread::sleep_for(milliseconds(500));
    }

    gpiod_line_release(trig);
    gpiod_line_release(echo);
    gpiod_chip_close(chip);

    return 0;
}
