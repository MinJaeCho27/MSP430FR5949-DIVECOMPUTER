/*
 * button.h
 *
 *  Created on: 2024. 5. 10.
 *      Author: 82105
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include "msp430fr5949.h"
#include "memory.h"
#include "lcd.h"
#include "timer.h"
#include "sensor.h"

void swtich_init(void);
void OFF(void);
void ON(void);

void switch_init(void)
{
    // 1번 스위치
    P1DIR &= ~BIT0;                 // P1.0를 input으로 설정
    P1REN |= BIT0;                  // P1.0 pull up/down enable
    P1OUT &= ~BIT0;                  // Set P1.0 pull-down resistor
    P1IFG &= ~BIT0;                 // interrupt flag 클리어
    P1IE |= BIT0;                   // interrupt 활성화 on P1.0
    P1IES &= ~BIT0;                  // falling edge에서 interrupt

    // 2번 스위치
    P3DIR &= ~BIT1;                 // P3.1을 input으로 설정
    P3REN |= BIT1;                  // P3.1 pull up/down enable
    P3OUT &= ~BIT1;                  // Set P3.1 pull-down resistor
    P3IFG &= ~BIT1;                 // interrupt flag 클리어
    P3IE |= BIT1;                   // interrupt 활성화 on P3.1
    P3IES &= ~BIT1;                  // falling edge에서 interrupt

    // 3번 스위치
    P1DIR &= ~BIT4;                 // P1.4을 input으로 설정
    P1REN |= BIT4;                  // P1.4 pull up/down enable
    P1OUT &= ~BIT4;                  // Set P1.4 pull-donw resistor
    P1IFG &= ~BIT4;                 // interrupt flag 클리어
    P1IE |= BIT4;                  // P1.4 interrupt 사용
    P1IES &= ~BIT4;                  // falling edge에서 interrupt


    // 4번 스위치
    P2DIR &= ~BIT5;               // P1.5을 input으로 설정
    P2REN |= BIT5;                // P2.5의 pull up/down enable
    P2OUT &= ~BIT5;                // Set P2.5 pull-down resistor
    P2IFG &= ~BIT5;                 // interrupt flag 클리어
    P2IE |= BIT5;                // P2.5 interrupt 사용
    P2IES &= ~BIT5;                  // falling edge에서 interrupt
}

void OFF (void)
{
    mode = MOD_OFF;
    mode_water = WATER_WAIT;
    timer0_disable();

    P2OUT &= ~BIT6;     // sensor off
    P2OUT &= ~BIT2;     // lcd  off
    P3OUT &= ~BIT5;     // 백라이트 끄기

    PMMCTL0 = (PMMPW | PMMREGOFF);      // 레귤레이터 off, pmm 잠금
    PM5CTL0 |= LOCKLPM5;                // lpm 3.5로 돌입하기 전 gpio 설정 잠금
    powerbutton = 0;
    button1_pressed = 0;
    button2_pressed = 0;
}

void ON (void)
{
    PMMCTL0 |= PMMPW;       // pmm unlock
    PMMCTL0 &= ~PMMREGOFF;
    PM5CTL0 &= ~LOCKLPM5;
}

#pragma vector = PORT1_VECTOR          // 1, 3번
__interrupt void switch1_pushed(void)
{
    //////////////////////1번 버튼////////////////////////////////
    if (P1IFG & BIT0)               // 1번 버튼이 눌린 경우
        {
            P1IFG &= ~BIT0;             // interrupt flag 클리어
            if (powerbutton == 0)
            {
                __bic_SR_register_on_exit(LPM3_bits + GIE);
                powerbutton = 1;
                button1_pressed = 1;
                button2_pressed = 0;
                mode = MOD_WATER;
                mode_water = WATER_WAIT;
            }

            else if (powerbutton == 1)
                OFF();

        }

    //////////////////////3번 버튼////////////////////////////////
    else if (P1IFG & BIT4)      // 3번 버튼 (P1.4) : 로그 페이지 넘기기
          {
              P1IFG &= ~BIT4;
              if (mode == MOD_LOG)
              {
                  if(*log_size_addr > 0)
                  {
                      if (log_page == 0)
                          log_page = 1;

                      else if (log_page == 1)
                          log_page = 0;
                  }

              }
          }

}

//////////////////////2번 버튼////////////////////////////////
#pragma vector = PORT3_VECTOR          // 2번 (P3.1)
__interrupt void switch3_pushed(void)
{
    if (P3IFG & BIT1)           // 2번 버튼이 눌린 경우 (P3.1 int flag high)
    {
        P3IFG &= ~BIT1;             // interrupt flag 클리어
        if (powerbutton == 0)
        {
            __bic_SR_register_on_exit(LPM3_bits + GIE);
            powerbutton = 1;
            button2_pressed = 1;
        }

        else if (powerbutton == 1)
        {
            if (mode == MOD_LOG)   // 로그모드에서 2번 : 다음 로그 보기
            {
                if (*log_size_addr > 0)
                {
                    if (++log_num == *log_size_addr)
                        log_num = 0;
                    log_page = 0;
                }
            }

            else   // 물 안에서 버튼 2번 : 백라이트 켜고 끄기
                P2OUT ^= BIT1;      // 백라이트 on/off 하는 코드
        }
    }
}

///////////////////////////4번 버튼/////////////////////////////
#pragma vector = PORT2_VECTOR       // 4번 (p2.5)
__interrupt void switch2_pushed(void)
{
    if (P2IFG & BIT5)      // 4번 버튼 : 로그 삭제
        {
            P2IFG &= ~BIT5;
            if(mode == MOD_LOG)
            {
                if (*log_size_addr > 0)
                {
                    delete_log(log_num);

                    if (*log_size_addr == 0)
                    {
                        clear_display();
                        show(text_log5);
                        __delay_cycles(100000);
                    }

                    else
                    {
                        if (log_num == *log_size_addr)
                            log_num--;
                    }
                }
            }
        }
}


#endif /* BUTTON_H_ */
