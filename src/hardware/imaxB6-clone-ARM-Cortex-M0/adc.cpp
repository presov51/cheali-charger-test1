/*
    cheali-charger - open source firmware for a variety of LiPo chargers
    Copyright (C) 2013  Paweł Stawicki. All right reserved.

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

#include "atomic.h"
#include "Hardware.h"
#include "imaxB6-pins.h"
#include "SMPS_PID.h"
#include "adc.h"
#include "Utils.h"
#include "memory.h"
#include "Settings.h"
#include "AnalogInputsPrivate.h"
#include "IO.h"
#include "SMPS.h"
#include "Discharger.h"


/* ADC - measurement:
 * uses Timer0 to trigger conversion
 * program flow:
 *     ADC routine                          |  multiplexer routine
 * -----------------------------------------------------------------------------
 * Timer0: start ADC (no MUX) conversion    |  1. switch MUX to Op-amp
 *                                          |  2. start discharging C_adc (MUX capacitor)
 *                                          |  3. wait 10us
 *                                          |  4. switch to MUX desired output, stop disch. C_adc
 *                                          |  (wait - for C_adc to charge)
 * ADC_vect:   switch ADC to MUX            |                             |
 *                                          |
 * Timer0: start ADC (MUX) conversion       |
 *                                          |
 *                                          |
 *                                          |
 *                                          |
 * ADC_vect:   switch ADC to no MUX         |
 *                                          |
 * Timer0: start ADC (MUX) conversion       |  repeat 1..4
 * ...
 *
 * note: 1-4 are in setMuxAddress()
 */


//discharge ADC capacitor on Vb6 - there is an operational amplifier
#define ADC_CAPACITOR_DISCHARGE_ADDRESS MADDR_V_BALANSER6
#define ADC_CAPACITOR_DISCHARGE_DELAY_US 10

namespace adc {

static uint8_t current_input_;

void initialize()
{
    //TODO:implement
    adc::reset();
}



struct adc_correlation {
    int8_t mux_;
    uint8_t adc_;
    AnalogInputs::Name ai_name_;
    bool trigger_PID_;
    bool slow_input_;
};

const adc_correlation order_analogInputs_on[] PROGMEM = {
    {MADDR_V_BALANSER_BATT_MINUS,   MUX0_Z_A_PIN,           AnalogInputs::Vb0_pin,         false, true},
    {-1,                            OUTPUT_VOLTAGE_MINUS_PIN,AnalogInputs::Vout_minus_pin,  false, false},
    {MADDR_V_BALANSER1,             MUX0_Z_A_PIN,           AnalogInputs::Vb1_pin,         false, true},
    {-1,                            SMPS_CURRENT_PIN,       AnalogInputs::Ismps,           true,  false},
    {MADDR_V_BALANSER2,             MUX0_Z_A_PIN,           AnalogInputs::Vb2_pin,         false, true},
    {-1,                            OUTPUT_VOLTAGE_PLUS_PIN,AnalogInputs::Vout_plus_pin,   false, false},
    {MADDR_V_BALANSER6,             MUX0_Z_A_PIN,           AnalogInputs::Vb6_pin,         false, false},
    {-1,                            SMPS_CURRENT_PIN,       AnalogInputs::Ismps,           true,  false},
    {MADDR_V_BALANSER5,             MUX0_Z_A_PIN,           AnalogInputs::Vb5_pin,         false, false},
    {-1,                            DISCHARGE_CURRENT_PIN,  AnalogInputs::Idischarge,      false, false},
    {MADDR_V_BALANSER4,             MUX0_Z_A_PIN,           AnalogInputs::Vb4_pin,         false, false},
    {-1,                            SMPS_CURRENT_PIN,       AnalogInputs::Ismps,           true,  false},
    {MADDR_V_BALANSER3,             MUX0_Z_A_PIN,           AnalogInputs::Vb3_pin,         false, false},
    {-1,                            V_IN_PIN,               AnalogInputs::Vin,             false, false},
    {MADDR_T_EXTERN,                MUX0_Z_A_PIN,           AnalogInputs::Textern,         false, false},
    {-1,                            SMPS_CURRENT_PIN,       AnalogInputs::Ismps,           true,  false},
};

AnalogInputs::Name getAIName(uint8_t input) {    return pgm::read(&order_analogInputs_on[input].ai_name_);}
bool getSlowInput(uint8_t input)           {    return pgm::read(&order_analogInputs_on[input].slow_input_);}
uint8_t getADC(uint8_t input)               {    return pgm::read(&order_analogInputs_on[input].adc_);}
bool getTriggerPID(uint8_t input)          {    return pgm::read(&order_analogInputs_on[input].trigger_PID_);}
int8_t getMUX(uint8_t input)                {    return pgm::read(&order_analogInputs_on[input].mux_);;}

inline uint8_t nextInput(uint8_t i) {
    if(++i >= sizeOfArray(order_analogInputs_on)) i=0;
    return i;
}


void setADC(uint8_t pin) {
    //TODO:implement
}

inline uint8_t getPortBAddress(int8_t address)
{
    //TODO:implement
//    return (PORTB & 0x1f) | (address & 7) << 5;
}

void setMuxAddress(int8_t address)
{
    if(address < 0)
        return;
    //TODO:implement
}

void processConversion(uint8_t input)
{
    uint8_t low, high;

    //TODO:implement
    //low  = ADCL;
    //high = ADCH;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        AnalogInputs::adc_[getAIName(input)] = (high << 8) | low;
    }
}

void reset() {
    current_input_ = 0;
}


void finalizeMeasurement()
{
    AnalogInputs::adc_[AnalogInputs::IsmpsValue]        = SMPS::getValue();
    AnalogInputs::adc_[AnalogInputs::IdischargeValue]   = Discharger::getValue();
    AnalogInputs::intterruptFinalizeMeasurement();
}

void setNextMuxAddress()
{
    uint8_t input = nextInput(current_input_);
    int8_t mux = getMUX(input);
    //TODO: disable temperature
    if(settings.UART_ != Settings::Disabled && mux == MADDR_T_EXTERN)
        mux = MADDR_V_BALANSER6;

    setMuxAddress(mux);
}


void timerInterrupt()
{
    setNextMuxAddress();
}

void conversionDone()
{
    processConversion(current_input_);
    current_input_ = nextInput(current_input_);
    setADC(getADC(current_input_));
    if(getTriggerPID(current_input_))
        SMPS_PID::update();

    if(AnalogInputs::isPowerOn() && current_input_ == 0)
        finalizeMeasurement();
}

} // namespace adc


//TODO:implement
/*ISR(TIMER0_COMP_vect)
{
    adc::timerInterrupt();
}

ISR(ADC_vect)
{
    adc::conversionDone();
}
*/


