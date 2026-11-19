#ifndef SERVO_PWM_H
#define SERVO_PWM_H

#include <stdint.h>

/* PA15 / TIM2_CH1，50Hz，500-2500us */
void Servo_Init(void);
void Servo_SetPulse(uint16_t us);
void Servo_SetAngle(float angle_deg);   /* 0-180 度 */

#endif /* SERVO_PWM_H */
