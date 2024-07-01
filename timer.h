/*
 * timer.h
 *
 *  Created on: 2024. 5. 10.
 *      Author: 82105
 */

#ifndef TIMER_H_
#define TIMER_H_

#include "msp430fr5949.h"
#include "memory.h"
#include "sensor.h"
#include "lcd.h"

inline void timer0_set(void);
inline void timer0_enable(void);        //timer 0 활성화
inline void timer0_disable(void);       //timer 0 비활성화

volatile unsigned int depth_before;
volatile unsigned int tick_1ms = 0;
volatile unsigned char pss;

volatile unsigned char powerbutton = 0;
volatile unsigned char button1_pressed = 0;
volatile unsigned char button2_pressed = 0;

inline void timer0_set(void)        // compare mode 세팅
{
    TA0CCR0 = 999;     // 1ms마다 인터럽트 발생하도록 compare mode
    TA0CTL = TASSEL_2 + MC_1;   // SMCLK 사용, 카운터 up mode(TA0CCR0까지 증가)
    tick_1ms = 0;
}

inline void timer0_enable(void)
{
    TA0CCTL0 |= CCIE;    // compare 인터럽트 허용
}

inline void timer0_disable(void)
{
    TA0CCTL0 &= ~CCIE;      // compare 인터럽트 x
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void sec_1(void)   // 1초마다 센서 입력 받음
{
    if(++tick_1ms >= 250)
    {
        if (mode == MOD_GOING)  // 다이빙 진행중이면
        {
            TA0CCTL0 &= ~CCIFG;      // 타이머 compare 인터럽트 플래그 clear

            __enable_interrupt();

            P1SEL1 |= BIT6;
            P1SEL0 &= ~BIT6;

            P1SEL1 |= BIT7;
            P1SEL0 &= ~BIT7;          // P1.6 SDA, P1.7 SCL로 기능하도록 설정

            UCB0CTLW0 |= UCSWRST;    // I2C 리셋(I2C 멈춤)
            UCB0CTLW0 |= (UCMST + UCSSEL_2 + UCMODE_3 + UCSYNC); // I2C 마스터 모드, 클럭 동기화, 서브클락 사용
            UCB0I2COA0 |= UCOAEN;       // 다중 슬레이브 허용
            UCB0BRW = 1;       // baud rate = SMCLK / 1 (331.25kHz)
            UCB0I2CSA = 0x76;   // 센서 주소

            UCB0CTLW0 &= ~UCSWRST;      // I2C 작동
            UCB0CTLW1 |= UCASTP_2;      // 자동 정지
            UCB0IE |= (UCTXIE0 | UCRXIE0 | UCNACKIE);  // 수신 송신 NACK 인터럽트 활성화

            sensor_enable();

            if (mode_water == WATER_WAIT)   // 다이빙 대기
            {
                clear_display();
                make_text_main();
                show(text_main1);
                nextline();
                show(text_main2);

                // 다이빙 시작
                if (depth_sensor > 5)   // 대기 도중 0.5미터 이상이면 다이빙 시작
                {
                    // buzzer로 시작 알림
                    PJOUT |= BIT4;
                    __delay_cycles(100000);
                    PJOUT &= ~BIT4;

                    mode_water = WATER_START;

                    pss = 1;
                }
            }

            if (mode_water == WATER_START)  // 다이빙 시작 시 세팅
            {
                *water_startTime = (RTCHOUR / 10) * 1000 + (RTCHOUR % 10) * 100
                        + (RTCMIN / 10) * 10 + (RTCMIN % 10);
                 tmp_sensor = 0;
                 depth_sensor = 0;
                 tmp_avg = 0;
                 depth_avg = 0;
                 tmp_min = 999;
                 depth_max = 0;
                 minute_water = 0;
                 second_water = 0;
                 diving_sec = 0;
                 countdown = 180;

                 mode_water = WATER_GOING;
            }

            if (mode_water == WATER_GOING)     // 다이빙 진행
            {
                if(depth_max >= 100)
                {
                    if (depth_sensor <= 50)   //다이빙 진행 도중 5미터 도달 시 타이머
                    {
                        if (pss == 1)
                        {
                            PJOUT |= BIT4;
                            __delay_cycles(10000);
                            PJOUT &= ~BIT4;

                            pss = 0;
                        }
                    }
                }

                if(pss == 0)
                    countdown--;

                if(depth_before - depth_sensor >= 1.5)        // 급상승 (15cm/s)
                    PJOUT |= BIT4;

                else if(depth_before - depth_sensor < 1.5)
                    PJOUT &= ~BIT4;

                if (depth_sensor < 4)   // 0.4미터 위로 올라오면 다이빙 종료
                {
                    // 부저
                    PJOUT |= BIT4;
                    __delay_cycles(10000);
                    PJOUT &= ~BIT4;
                    __delay_cycles(5000);
                    PJOUT |= BIT4;
                    __delay_cycles(10000);
                    PJOUT &= ~BIT4;

                    mode_water = WATER_FINISH;  // finish mode
                }
            }

            if (mode_water == WATER_GOING)
            {
                if (tmp_sensor != 0 && tmp_min > tmp_sensor)    //  온도가 0이 아니고 최저 수온보다 측정 수온이 작으면
                    tmp_min = tmp_sensor;       // 최저 수온 업데이트

                if (depth_max < depth_sensor)
                    depth_max = depth_sensor;       // 최대 수심 업데이트

                if (diving_sec == 0)            // 평균 기록
                {
                    tmp_avg = tmp_sensor;
                    depth_avg = depth_sensor;
                }
                else
                {

                    long long temp_tmp_avg = (long long) tmp_avg * diving_sec + tmp_sensor;
                    long long temp_depth_avg = (long long) depth_avg * diving_sec
                            + depth_sensor;

                    tmp_avg = (unsigned int) (temp_tmp_avg / (diving_sec + 1));
                    depth_avg = (unsigned int) (temp_depth_avg / (diving_sec + 1));
                }

                diving_sec++;

                if (++second_water == 60)
                {
                    second_water = 0;
                    minute_water++;
                }

                clear_display();
                make_text_water();
                show(text_water1);
                nextline();
                show(text_water2);

                depth_before = depth_sensor;
            }

            if (mode_water == WATER_FINISH)    // 다이빙 종료
            {
                mode_water = WATER_WAIT;
                insert_log();   // 로그 입력

                minute_water = 0;
                second_water = 0;
                diving_sec = 0;
                clear_display();        // 값, lcd 초기화

                mode = MOD_OFF;

                P2OUT &= ~BIT6;     // sensor off
                P2OUT &= ~BIT2;     // lcd  off
                P2OUT &= ~BIT1;     // 백라이트 끄기

                powerbutton = 0;
                button1_pressed = 0;
                button2_pressed = 0;

                PMMCTL0 = (PMMPW | PMMREGOFF);      // 레귤레이터 off, pmm 잠금
                PM5CTL0 |= LOCKLPM5;                // lpm 3.5로 돌입하기 전 gpio 설정 잠금

                __bis_SR_register(LPM3_bits + GIE);     // lpm3.5
            }

            timer0_enable();        // 다시 timer on
        }
        tick_1ms = 0;
    }
}

#endif /* TIMER_H_ */
