#include <unistd.h>
#include <iostream>
#include <cmath>
#include "unitreeMotor/unitreeMotor.h"
#include "serialPort/SerialPort.h"

const char* USB_LA_Motor = "/dev/ttyUSB2";

void print_MotorData(MotorCmd &cmd, MotorData &data);
void move90degStep(int motor_id, int direction, MotorCmd &cmd, MotorData &data, SerialPort &serial);

int main() {
    SerialPort serial_LA(USB_LA_Motor);

    MotorCmd cmd1;
    MotorData data1;
    cmd1.motorType = MotorType::GO_M8010_6;
    data1.motorType = MotorType::GO_M8010_6;

    int motor_id = 1;   // change if needed
    int input = -1;

    std::cout << "Motor Step Program\n";
    std::cout << "Enter 1 for +90 deg, 2 for -90 deg, 0 to exit\n";

    while (true) {
        std::cout << "> ";
        std::cin >> input;

        if (!std::cin) {
            std::cout << "Invalid input\n";
            break;
        }

        if (input == 0) {
            break;
        }
        else if (input == 1) {
            move90degStep(motor_id, +1, cmd1, data1, serial_LA);
        }
        else if (input == 2) {
            move90degStep(motor_id, -1, cmd1, data1, serial_LA);
        }
        else {
            std::cout << "Unknown command\n";
        }
    }

    cmd1.id = motor_id;
    cmd1.mode = 0;
    serial_LA.sendRecv(&cmd1, &data1);

    return 0;
}
/*
void move90degStep(int motor_id, int direction, MotorCmd &cmd, MotorData &data, SerialPort &serial) {
    const double GEAR_RATIO = 6.33;
    const double STEP_OUTPUT_RAD = M_PI / 4.0;     // 90 degrees at output
    const double STEP_MOTOR_RAD = STEP_OUTPUT_RAD * GEAR_RATIO;
    const double POS_TOL_RAD = 0.05 * GEAR_RATIO;
    const int LOOP_DELAY_US = 10000;

    // Read current position
    cmd.id = motor_id;
    cmd.mode = 0;
    serial.sendRecv(&cmd, &data);

    if (!data.correct) {
        std::cout << "Failed to read current position\n";
        return;
    }
    // new version
    double q_cmd = data.q;
    const double MAX_STEP = 0.01;  // rad per cycle (tune this!)
    
    while (true) {
        double error = q_target - q_cmd;
    
        if (std::fabs(error) < MAX_STEP) {
            q_cmd = q_target;
        } else {
            q_cmd += (error > 0 ? MAX_STEP : -MAX_STEP);
        }
    
        cmd.id   = motor_id;
        cmd.mode = 1;
        cmd.kp   = 0.1;     // you can increase kp now
        cmd.kd   = 0.02;    // add damping!
        cmd.q    = q_cmd;
        cmd.dq   = 0.0;
        cmd.tau  = 0.0;
    
        serial.sendRecv(&cmd, &data);
    
        if (data.correct) {
            print_MotorData(cmd, data);
    
            if (std::fabs(q_target - data.q) < POS_TOL_RAD) {
                break;
            }
        }
    
        usleep(LOOP_DELAY_US);
    }
  
    // Stop motor
    cmd.id = motor_id;
    cmd.mode = 0;
    serial.sendRecv(&cmd, &data);

    std::cout << "Reached target.\n\n";
}
*/

void print_MotorData(MotorCmd &cmd, MotorData &data) {
    std::cout << "motor ID:    " << cmd.id << "\n";
    std::cout << "motor pos:   " << data.q << " rad\n";
    std::cout << "motor temp:  " << data.temp << " C\n";
    std::cout << "motor speed: " << data.dq << " rad/s\n";
    std::cout << "motor tau:   " << data.tau << " Nm\n";
    std::cout << std::endl;
}


void move90degStep(int motor_id, int direction, MotorCmd &cmd, MotorData &data, SerialPort &serial) {
    const double GEAR_RATIO   = 6.33;
    const double STEP_OUTPUT_RAD = M_PI / 2.0;   // 90 deg output
    const double STEP_MOTOR_RAD  = STEP_OUTPUT_RAD * GEAR_RATIO;

    const double POS_TOL_RAD = 0.05 * GEAR_RATIO;

    const double MAX_STEP = 0.01;   // rad per cycle
    const int LOOP_DELAY_US = 10000;

    // Read current position
    cmd.id = motor_id;
    cmd.mode = 0;

    serial.sendRecv(&cmd, &data);

    if (!data.correct) {
        std::cout << "Failed to read current position\n";
        return;
    }

    // Current position
    double q_now = data.q;

    // Target position
    double q_target = q_now + direction * STEP_MOTOR_RAD;

    // Commanded position
    double q_cmd = q_now;

    while (true) {

        double error = q_target - q_cmd;

        // Smooth ramp toward target
        if (std::fabs(error) < MAX_STEP) {
            q_cmd = q_target;
        }
        else {
            q_cmd += (error > 0 ? MAX_STEP : -MAX_STEP);
        }

        // Send position command
        cmd.id   = motor_id;
        cmd.mode = 1;

        cmd.kp   = 0.1;
        cmd.kd   = 0.02;

        cmd.q    = q_cmd;
        cmd.dq   = 0.0;
        cmd.tau  = 0.0;

        serial.sendRecv(&cmd, &data);

        if (data.correct) {

            print_MotorData(cmd, data);

            // Check actual motor position
            if (std::fabs(q_target - data.q) < POS_TOL_RAD) {
                break;
            }
        }

        usleep(LOOP_DELAY_US);
    }

    // Stop motor
    cmd.id = motor_id;
    cmd.mode = 0;

    serial.sendRecv(&cmd, &data);

    std::cout << "Reached target.\n\n";
}
