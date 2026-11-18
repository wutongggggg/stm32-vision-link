/*
 * WonderCam 视觉伺服闭环 v2
 * - 修: 禁用 JTAG(保留SWD), 释放 PA15 给 TIM2_CH1
 * - 上电先让舵机 1000->2000->1500 扫动一次(自检)
 * - 之后跟随: 读CX -> 偏差 -> 舵机
 * soft I2C PB0=SCL PB1=SDA (地址低字节先), dev 0x32. USART1 115200.
 */
#include "stm32f1xx.h"
static void delay(volatile uint32_t n){ while(n--) __NOP(); }
static void uart_init(void){ RCC->APB2ENR|=RCC_APB2ENR_IOPAEN|RCC_APB2ENR_USART1EN; GPIOA->CRH=(GPIOA->CRH&~(0xFu<<4))|(0xBu<<4); USART1->BRR=0x45; USART1->CR1=USART_CR1_UE|USART_CR1_TE; }
static void upc(char c){ while(!(USART1->SR&USART_SR_TXE)){} USART1->DR=(uint8_t)c; }
static void ups(const char*s){ while(*s)upc(*s++); }
static void putu(uint32_t v){ char b[12]; int i=0; if(!v){upc('0');return;} while(v){b[i++]=(char)('0'+v%10);v/=10;} while(i)upc(b[--i]); }
static void puti(int32_t v){ if(v<0){upc('-'); putu((uint32_t)(-v));} else putu((uint32_t)v); }

static void i2c_gpio(void){ RCC->APB2ENR|=RCC_APB2ENR_IOPBEN; GPIOB->CRL=(GPIOB->CRL&~0xFFu)|0x77u; GPIOB->ODR|=(1u<<0)|(1u<<1); delay(2000); }
static inline void sclh(void){GPIOB->ODR|=(1u<<0);} static inline void scll(void){GPIOB->ODR&=~(1u<<0);}
static inline void sdah(void){GPIOB->ODR|=(1u<<1);} static inline void sdal(void){GPIOB->ODR&=~(1u<<1);}
static inline int sdard(void){return (GPIOB->IDR>>1)&1u;}
static void dly(void){ for(volatile int i=0;i<30;i++)__NOP(); }
static void i2c_start(void){ sdah();sclh();dly();sdal();dly();scll();dly(); }
static void i2c_stop(void){ sdal();dly();sclh();dly();sdah();dly(); }
static int i2c_w(uint8_t b){ for(int i=7;i>=0;i--){ if(b&(1<<i))sdah();else sdal(); dly();sclh();dly();scll();dly(); } sdah();dly();sclh();dly(); int a=sdard(); scll();dly(); return a; }
static uint8_t i2c_r(int ack){ uint8_t v=0; sdah(); for(int i=0;i<8;i++){ sclh();dly(); v<<=1; if(sdard())v|=1; scll();dly(); } if(ack)sdal();else sdah(); dly();sclh();dly();scll();dly();sdah(); return v; }
static int i2c_read(int dev7,uint16_t reg,uint8_t*buf,int n){
    i2c_start(); if(i2c_w((uint8_t)(dev7<<1))){i2c_stop();return -1;}
    if(i2c_w((uint8_t)(reg&0xFF))){i2c_stop();return -2;}
    if(i2c_w((uint8_t)(reg>>8))){i2c_stop();return -3;}
    i2c_start(); if(i2c_w((uint8_t)((dev7<<1)|1))){i2c_stop();return -4;}
    for(int i=0;i<n;i++) buf[i]=i2c_r(i<(n-1)?1:0);
    i2c_stop(); return 0;
}
#define WC 0x32

static void servo_init(void){
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    /* 一次性写 AFIO->MAPR: 禁 JTAG(SWJ_CFG=010,保留SWD) + TIM2部分重映射1(CH1->PA15) */
    uint32_t m = AFIO->MAPR;
    m &= ~((0x7u<<24) | (0x3u<<8));
    m |=  (0x2u<<24) | (0x1u<<8);
    AFIO->MAPR = m;
    /* PA15 复用推挽 50MHz: CRH[31:28]=0xB */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu<<28)) | (0xBu<<28);
    TIM2->PSC = 7;                 /* 8MHz/8 = 1MHz (1us) */
    TIM2->ARR = 19999;             /* 20ms -> 50Hz */
    TIM2->CCMR1 = (0x6u<<4) | (1u<<3);
    TIM2->CCER |= (1u<<0);
    TIM2->CCR1 = 1500;
    TIM2->EGR = 1;
    TIM2->CR1 |= (1u<<0);
}
static void servo_pulse(uint16_t us){ if(us<500)us=500; if(us>2400)us=2400; TIM2->CCR1=us; }
static void wait(volatile uint32_t n){ while(n--) __NOP(); }

#define CENTER_X 160
#define GAIN     5.0f
#define SERVO_SIGN (+1)

int main(void){
    uart_init(); i2c_gpio(); servo_init(); delay(300000);
    ups("\r\n=== SERVO FOLLOW v2 (JTAG off, PA15) ===\r\n");
    /* 上电自检扫动 */
    ups("SWEEP 1000..2000..1500\r\n");
    servo_pulse(1000); wait(2500000);
    servo_pulse(2000); wait(2500000);
    servo_pulse(1500); wait(2500000);
    ups("SWEEP done -> follow\r\n");

    float cur = 1500.0f;
    while(1){
        uint8_t num=0,id=0,raw[8];
        i2c_read(WC,0x1000,&num,1); delay(8000);
        if(i2c_read(WC,0x1001,&num,1)==0 && num>0 && i2c_read(WC,0x1030,raw,8)==0){
            i2c_read(WC,0x1002,&id,1);
            int16_t x=(int16_t)(raw[0]|(raw[1]<<8));
            int16_t w=(int16_t)(raw[4]|(raw[5]<<8));
            int32_t cx = x + w/2;
            int32_t err = cx - CENTER_X;
            if(err>-15 && err<15) err=0;
            float target = 1500.0f + SERVO_SIGN*err*GAIN;
            if(target<600) target=600; if(target>2400) target=2400;
            cur += (target-cur)*0.25f;
            servo_pulse((uint16_t)cur);
            ups("CX="); puti(cx); ups(" err="); puti(err); ups(" pulse="); putu((uint32_t)cur); ups("\r\n");
        } else {
            ups("no color (hold)\r\n");
        }
        delay(80000);
    }
}
