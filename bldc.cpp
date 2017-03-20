/*
	Copyright 2017 Ryan Owens

	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

/*
 * bldc.cpp
 *
 *  Created on: 19 mar 2017
 *      Author: Ryan
 */
#include "bldc.h"
#include "comm_uart.h"
#include "bldc_interface.h"
#include "bldc_interface_uart.h"
#include <unistd.h>
#include <stdio.h>

// Constant declarations
const RxData zeroRx = {}; // rxData = 0

// global variables
static RxData rxUART1 = {}; // raw UART1 data
static bool dataRdy = false; // true indicates that data has been received from UART 
                             // but not stored in rxData

//********************************************************************************
// Pre: Serial device is enabled
// Post: File descriptor for read/write to serial device is opened. Rx data
//       callback functions have been set.
//********************************************************************************
static void init_vesc_uart(void) {
    comm_uart_init();
	bldc_interface_set_rx_value_func(bldc_val_received);
	// Won't work unless VESC firmware modified
	bldc_interface_set_rx_rotor_pos_func(bldc_pos_received);
}
//********************************************************************************
// Pre: Serial port file descriptor is open.
// Post: File descriptor has been closed.
//********************************************************************************
static void close_vesc_uart(void) {
	comm_uart_close();
}
//********************************************************************************
// Pre: rx_rotor_pos_func callback has been set
// Post: Rotor position in degrees is printed to console.
//       Will add pos data member instead of print after VESC firmware mod complete.
//********************************************************************************
void bldc_pos_received(float pos) {
	printf("\r\n");
	printf("Rotor position: %.2f degrees\r\n", pos);
	printf("\r\n");
}
//********************************************************************************
// Pre: rx_value_func callback has been set.
// Post: Motor data is stored in global UART Rx buffer and
//       flag has been set indicating that data is available in buffer.
//********************************************************************************
void bldc_val_received(mc_values *val) {
	// set raw UART rx struct
	rxUART1.voltageIn = val->v_in;
	rxUART1.tempPCB = val->temp_pcb;
	rxUART1.tempMOS1 = val->temp_mos1;
	rxUART1.tempMOS2 = val->temp_mos2;
	rxUART1.tempMOS3 = val->temp_mos3;
	rxUART1.tempMOS4 = val->temp_mos4;
	rxUART1.tempMOS5 = val->temp_mos5;
	rxUART1.tempMOS6 = val->temp_mos6;
	rxUART1.currentMotor = val->current_motor;
	rxUART1.currentIn = val->current_in;
	rxUART1.rpm = val->rpm;
	rxUART1.duty = val->duty_now;
	rxUART1.ampHours = val->amp_hours;
	rxUART1.ampHoursCharged = val->amp_hours_charged;
	rxUART1.wattHours = val->watt_hours;
	rxUART1.wattHoursCharged = val->watt_hours_charged;
	rxUART1.tachometer = val->tachometer;
	rxUART1.tachometerAbs = val->tachometer_abs;
	rxUART1.faultCode = bldc_interface_fault_to_string(val->fault_code);
	dataRdy = true;
}
//********************************************************************************
// Pre: For UART: UART1 is enabled by uEnv.txt
// Post: UART interface is initialized.
//********************************************************************************
void BLDC::init(void) {
	// Initialize UART1
    comm_uart_init();
	// Set rx data callback functions
	bldc_interface_set_rx_value_func(bldc_val_received);
	// Won't work unless VESC firmware modified
	bldc_interface_set_rx_rotor_pos_func(bldc_pos_received);
}
//********************************************************************************
// Pre: UART fd is open.
// Post: UART fd is closed for reading and writing.
//********************************************************************************
void BLDC::close() {
	comm_uart_close();
}
//********************************************************************************
// Pre: Serial port is initialized. vescID value is input.
// Post: Motor object instantiated with ID, zero rxData, and motor config.
//********************************************************************************
BLDC::BLDC(VescID vescID, Motor_Config motorConfig) : id(vescID), rxData(zeroRx), config(motorConfig) {
}
//********************************************************************************
//********************************************************************************
BLDC::~BLDC() {
}
//********************************************************************************
// Pre: Serial port is initialized. Speed in RPM is input.
// Post: Speed of motor has been set in RPM.
//********************************************************************************
void BLDC::set_Speed(int rpm) {
    // brake if joystick is within neutral range
    if (rpm > -1*config.Min_Erpm && rpm < config.Min_Erpm) {
		// apply brake if braking enabled
		if (config.enable_brake) {
			bldc_interface_set_forward_can(id);
        	apply_Brake(config.Brake_Current);
        	return;
		}
		else
			rpm = 0;
	}
	/*
    // filter values between 0 and config.Min_Erpm
    if (rpm < config.Min_Erpm && rpm > 0)
        rpm = config.Min_Erpm;
    else if (rpm > -1*config.Min_Erpm && rpm < 0)
        rpm = -1*config.Min_Erpm;*/
    // set controller id and send the packet
    bldc_interface_set_forward_can(id);
    bldc_interface_set_rpm(rpm);
}
//********************************************************************************
// Pre: Serial port is initialized. Unscaled Analog/Digital value is input.
// Post: Speed of motor has been set in RPM.
//********************************************************************************
void BLDC::set_Speed_Unscaled(float val) {
    int rpm;
    // scale val to rpm value
    rpm = scale_To_Int(val, -1*config.Max_Erpm, config.Max_Erpm);
    set_Speed(rpm);
}
//********************************************************************************
// Pre: Serial port is initialized. Value in amps is input.
// Post: Current of motor has been set in Amps.
//********************************************************************************
void BLDC::set_Current(float amps) {
    // brake if joystick is within neutral range
    if (amps > -1*config.Min_Amps && amps < config.Min_Amps) {
		// apply brake if braking enabled
		if (config.enable_brake) {
			bldc_interface_set_forward_can(id);
        	apply_Brake(config.Brake_Current);
        	return;
		}
		else
			amps = 0;
	}
	/*
    // filter values between 0 and config.Min_Amps
    if (amps < config.Min_Amps && amps > 0)
        amps = config.Min_Amps;
    else if (amps > -1*config.Min_Amps && amps < 0)
        amps = -1*config.Min_Amps;*/
    // set controller id and send the packet
    bldc_interface_set_forward_can(id);
    bldc_interface_set_current(amps);
}
//********************************************************************************
// Pre: Serial port is initialized. Unscaled Analog/Digital value is input.
// Post: Current of motor has been set in Amps.
//********************************************************************************
void BLDC::set_Current_Unscaled(float val) {
    float amps;
    // scale val to current value
    amps = scale_To_Float(val, -1*config.Max_Amps, config.Max_Amps);
    set_Current(amps);
}
//********************************************************************************
// Pre: Serial port is initialized. Brake current in amps is input.
// Post: Braking has been applied to motor with given current.
//********************************************************************************
void BLDC::apply_Brake(float brake) {
    // No scaling required
    bldc_interface_set_forward_can(id);
    bldc_interface_set_current_brake(brake);
}
//********************************************************************************
// Pre: Serial port is initialized. Duty cycle is input as percentage.
// Post: Duty cycle of motor has been set (percentage).
//********************************************************************************
void BLDC::set_Duty(float duty) {
    // brake if joystick is within neutral range
    if (duty > -1*config.Min_Duty && duty < config.Min_Duty) {
		// apply brake if braking enabled
		if (config.enable_brake) {
			bldc_interface_set_forward_can(id);
        	apply_Brake(config.Brake_Current);
        	return;
		}
		else
			duty = 0;
    }   
    // set controller id and send the packet
    bldc_interface_set_forward_can(id);
    bldc_interface_set_duty_cycle(duty);
}
//********************************************************************************
// Pre: Serial port is initialized. Unscaled Analog/Digital value is input.
// Post: Duty cycle of motor has been set (percentage).
//********************************************************************************
void BLDC::set_Duty_Unscaled(float val) {
	float duty;
	duty = scale_To_Float(duty, -1*config.Max_Duty, config.Max_Duty);
	set_Duty(duty);
}
//********************************************************************************
// Pre: Serial port is initialized. Position 0-360 degrees is input.
// Post: Position of motor has been set in in degrees.
//********************************************************************************
void BLDC::set_Pos(float pos) {
    // set controller id and send the packet
    bldc_interface_set_forward_can(id);
    bldc_interface_set_pos(pos);
}
//********************************************************************************
// Pre: Rx callback functions have been set.
// Post: Data has been read from UART and stored in rxData.
//********************************************************************************
void BLDC::get_Values(void) {
	bldc_interface_set_forward_can(id);
	bldc_interface_get_values();
	usleep(15000); // wait for data to become available on UART
	read_Data();
}
//********************************************************************************
// Pre: Rx callback functions have been set. Also change VESC firmware.
// Post: Rotor position callback function is invoked (pos printed to console).
//********************************************************************************
void BLDC::get_Pos(void) {
	// Position read over UART requires non-standard firmware
	bldc_interface_set_forward_can(id);
	bldc_interface_get_rotor_pos();
}
//********************************************************************************
// Pre: Rx callback functions have been set.
// Post: UART Rx buffer has been read for available data. 
//       If available, data has been read from UART and stored in raw UART struct.
//********************************************************************************
bool BLDC::read_Data(void) {
	bool ret = false; // return true if read completes successfully
	// read data from UART
	receive_packet();
	// if data from UART is available,
	// store UART data in rxData
	if (dataRdy) {
		rxData = rxUART1;
		dataRdy = false; // data has been read. UART1 rx no longer rdy for reading
		rxUART1 = zeroRx; // zero out the raw UART rx data
		ret = true;
	}
	// reset the packet handler in case data did not transfer correctly
	bldc_interface_uart_run_timer();
	return ret;
}
//********************************************************************************
// Pre: Motor is spinning and command has not been given before timeout occurs.
// Post: VESC timer has been reset and motor keeps spinning.
//********************************************************************************
void BLDC::send_Alive(void) {
	bldc_interface_set_forward_can(id);
	bldc_interface_send_alive();
}
//********************************************************************************
// Pre: None.
// Post: rxData printed to console.
//********************************************************************************
void BLDC::print_Data(void) {
	printf("\r\n");
	printf("Input voltage: %.2f V\r\n", rxData.voltageIn);
	printf("Temp PCB:      %.2f degC\r\n", rxData.tempPCB);
	printf("Temp MOSFET1:  %.2f degC\r\n", rxData.tempMOS1);
	printf("Temp MOSFET2:  %.2f degC\r\n", rxData.tempMOS2);
	printf("Temp MOSFET3:  %.2f degC\r\n", rxData.tempMOS3);
	printf("Temp MOSFET4:  %.2f degC\r\n", rxData.tempMOS4);
	printf("Temp MOSFET5:  %.2f degC\r\n", rxData.tempMOS5);
	printf("Temp MOSFET6:  %.2f degC\r\n", rxData.tempMOS6);
	printf("Current motor: %.2f A\r\n", rxData.currentMotor);
	printf("Current in:    %.2f A\r\n", rxData.currentIn);
	printf("RPM:           %.1f RPM\r\n", rxData.rpm);
	printf("Duty cycle:    %.1f %%\r\n", rxData.duty);
	printf("Ah Drawn:      %.4f Ah\r\n", rxData.ampHours);
	printf("Ah Regen:      %.4f Ah\r\n", rxData.ampHoursCharged);
	printf("Wh Drawn:      %.4f Wh\r\n", rxData.wattHours);
	printf("Wh Regen:      %.4f Wh\r\n", rxData.wattHoursCharged);
	printf("Tacho:         %i counts\r\n", rxData.tachometer);
	printf("Tacho ABS:     %i counts\r\n", rxData.tachometerAbs);
	printf("Fault Code:    %s\r\n", rxData.faultCode.c_str());
	printf("\r\n");
}
//********************************************************************************
// Pre: Value to scale, min and max values for mapping have been input.
// Post: Value has been scaled and returned as float.
//********************************************************************************
float BLDC::scale_To_Float(float val, float min, float max) {
    return (max-min)/(config.Scale_Max-config.Scale_Min) * val;
}
//********************************************************************************
// Pre: Value to scale, min and max values for mapping have been input.
// Post: Value has been scaled to and returned as integer.
//********************************************************************************
int BLDC::scale_To_Int(float val, int min, int max) {
    return ((max-min)/(config.Scale_Max-config.Scale_Min)) * val;
}