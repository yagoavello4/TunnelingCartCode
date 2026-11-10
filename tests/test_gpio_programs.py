"""Hardware-free regression tests. Run: python3 -m unittest discover -s tests -v"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HEADER = r'''
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cerrno>
struct gpiod_chip {};
struct gpiod_line { unsigned int pin; };
inline int event(const char* kind, unsigned int pin, int value) {
    std::printf("GPIO %s %u %d\n", kind, pin, value);
    static int count = 0;
    ++count;
    const char* fail = std::getenv("FAIL_EVENT");
    if (fail && count == std::atoi(fail)) { errno = EIO; return -1; }
    return 0;
}
inline gpiod_chip* gpiod_chip_open_by_name(const char*) {
    static gpiod_chip chip; return &chip;
}
inline gpiod_line* gpiod_chip_get_line(gpiod_chip*, unsigned int pin) {
    static gpiod_line lines[32]; lines[pin].pin = pin; return &lines[pin];
}
inline int gpiod_line_request_output(gpiod_line* line, const char*, int value) {
    return event("request", line->pin, value);
}
inline int gpiod_line_set_value(gpiod_line* line, int value) {
    return event("set", line->pin, value);
}
inline void gpiod_line_release(gpiod_line* line) { event("release", line->pin, 0); }
inline void gpiod_chip_close(gpiod_chip*) { event("close", 0, 0); }
'''
# Eliminate delays only in the test executables.
DELAYS = r'''
extern "C" unsigned int sleep(unsigned int) { return 0; }
extern "C" int usleep(unsigned int) { return 0; }
'''


class GpioPrograms(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.build = Path(cls.temp.name)
        (cls.build / "gpiod.h").write_text(HEADER)
        (cls.build / "delays.cpp").write_text(DELAYS)
        for name, source in [("actuator", "MotorControl/LinearActuatorTest.cpp"),
                             ("toggle", "GPIO_exp/gpio_test1.cpp")]:
            subprocess.run(["g++", "-std=c++14", "-Wall", "-Wextra", "-Werror",
                            "-I", str(cls.build), str(ROOT / source),
                            str(cls.build / "delays.cpp"), "-o", str(cls.build / name)],
                           check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_program(self, name, commands="", fail=None):
        env = os.environ.copy()
        env.pop("FAIL_EVENT", None)
        if fail is not None:
            env["FAIL_EVENT"] = str(fail)
        p = subprocess.run([str(self.build / name)], input=commands,
                           capture_output=True, text=True, env=env, timeout=2)
        events = [line for line in p.stdout.splitlines() if line.startswith("GPIO ")]
        return p, events

    def test_eof_does_not_repeat_motion(self):
        p, e = self.run_program("actuator", "1\n")
        self.assertEqual(p.returncode, 0)
        self.assertEqual(e.count("GPIO set 10 1"), 1)
        self.assertEqual(e[-4:], ["GPIO set 10 0", "GPIO release 17 0",
                                 "GPIO release 10 0", "GPIO close 0 0"])

    def test_bad_input_stops(self):
        p, e = self.run_program("actuator", "2\nbad\n")
        self.assertEqual(p.returncode, 1)
        self.assertEqual(e.count("GPIO set 10 1"), 1)
        self.assertEqual(e[-4], "GPIO set 10 0")

    def test_unknown_command_cuts_power(self):
        p, e = self.run_program("actuator", "4\n99\n0\n")
        self.assertEqual(p.returncode, 0)
        self.assertEqual(e[2:4], ["GPIO set 10 1", "GPIO set 10 0"])

    def test_direction_request_failure_releases_only_power(self):
        p, e = self.run_program("actuator", "1\n", fail=2)
        self.assertEqual(p.returncode, 1)
        self.assertEqual(e[2:], ["GPIO release 10 0", "GPIO close 0 0"])

    def test_failed_power_off_never_changes_direction(self):
        p, e = self.run_program("actuator", "2\n", fail=3)
        self.assertEqual(p.returncode, 1)
        self.assertNotIn("GPIO set 17 1", e)
        self.assertNotIn("GPIO set 10 1", e)
        self.assertEqual(e[-4], "GPIO set 10 0")

    def test_failed_direction_never_enables_power(self):
        p, e = self.run_program("actuator", "2\n", fail=4)
        self.assertEqual(p.returncode, 1)
        self.assertNotIn("GPIO set 10 1", e)
        self.assertEqual(e[-4], "GPIO set 10 0")

    def test_toggle_completes_ten_cycles(self):
        p, e = self.run_program("toggle")
        self.assertEqual(p.returncode, 0)
        self.assertEqual(e.count("GPIO set 17 1"), 10)
        self.assertEqual(e[-3:], ["GPIO set 17 0", "GPIO release 17 0", "GPIO close 0 0"])

    def test_toggle_write_failure_stops_and_retries_low(self):
        for event in [2, 3]:
            with self.subTest(event=event):
                p, e = self.run_program("toggle", fail=event)
                self.assertEqual(p.returncode, 1)
                self.assertEqual(e.count("GPIO set 17 1"), 1)
                self.assertEqual(e[-3:], ["GPIO set 17 0", "GPIO release 17 0", "GPIO close 0 0"])

    def test_shutdown_write_failure_returns_failure(self):
        p, _ = self.run_program("actuator", "0\n", fail=3)
        self.assertEqual(p.returncode, 1)


if __name__ == "__main__":
    unittest.main()
