#ifndef __ACTUATORS_H__
#define __ACTUATORS_H__

#include "main.h"

typedef struct {
    uint8_t pump_on;
    uint8_t fan_on;
    uint8_t pest_lamp_on;
    uint8_t buzzer_mode;
    uint8_t sky_window_state;
} ActuatorState_t;

void Actuators_Init(void);
void Actuators_GetState(ActuatorState_t *state);

void Pump_On(void);
void Pump_Off(void);
void Pump_Toggle(void);

void Fan_On(void);
void Fan_Off(void);
void Fan_Toggle(void);

void PestLamp_On(void);
void PestLamp_Off(void);
void PestLamp_Toggle(void);

void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint16_t ms);
void Buzzer_SetMode(uint8_t mode);
void Buzzer_Process(void);

void SkyWindow_Open(void);
void SkyWindow_HalfOpen(void);
void SkyWindow_FullOpen(void);
void SkyWindow_Close(void);
void SkyWindow_SetState(uint8_t state);
void SkyWindow_SetAngle(uint8_t angle_deg);

#endif
