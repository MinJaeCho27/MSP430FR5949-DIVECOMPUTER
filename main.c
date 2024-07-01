/**
 * main.c
 */

#include "msp430fr5949.h"
#include "memory.h"
#include "lcd.h"
#include "button.h"
#include "timer.h"
#include "sensor.h"

void main(void)
{
    power_init();       // power 세팅 (initialize)

    switch_init();      // 버튼 세팅 (initialize)

    lcd_init();         // lcd 세팅 (initialize)

    P2OUT |= BIT0;

    P1SEL1 |= BIT6;
    P1SEL0 &= ~BIT6;

    P1SEL1 |= BIT7;
    P1SEL0 &= ~BIT7;          // P1.6 SDA, P1.7 SCL로 기능하도록 설정

    UCB0CTLW0 |= UCSWRST;    // I2C 리셋(I2C 멈춤)
    UCB0CTLW0 |= UCMST + UCSSEL_2 + UCMODE_3 + UCSYNC; // I2C 마스터 모드, 클럭 동기화, 서브클락 사용
    UCB0I2COA0 |= UCOAEN;       // 다중 슬레이브 허용
    UCB0BRW = 1;       // baud rate = SMCLK / 1
    UCB0CTLW0 &= ~UCSWRST;      // I2C 작동
    UCB0CTLW1 |= UCASTP_2;      // 자동 정지
    UCB0IE |= (UCTXIE0 | UCRXIE0| UCNACKIE);  // 수신 송신 인터럽트 활성화

    P2OUT |= BIT2;     // lcd on

    make_text_main();
    show(text_main1);
    nextline();
    show(text_main2);
    clear_display();

    while (1)
        {
            if (powerbutton)
            {
                //////////////////// 1번 버튼 //////////////////////////
                if(button1_pressed)
                {
                    ON();
                    timer0_set();
                    timer0_enable();

                    mode = MOD_GOING;
                    mode_water = WATER_WAIT;

                    P2OUT |= BIT2;     // lcd  on
                    P2OUT |= BIT6;     // sensor on
                    button1_pressed = 0;
                }

                if(mode == WATER_GOING)
                {
                    if (mode_water == WATER_WAIT)
                    {
                        clear_display();
                        make_text_main();
                        show(text_main1);
                        nextline();
                        show(text_main2);
                    }

                    else if (mode_water == WATER_GOING)
                    {
                        clear_display();
                        make_text_water();
                        show(text_water1);
                        nextline();
                        show(text_water2);
                    }
                }

                /////////////// 2번 버튼 (로그모드, 로그모드에서 다음 로그) //////////////////////////
                else if (button2_pressed)
                {
                    if (mode == MOD_OFF)        // 저전력에서 2번 : 로그모드
                    {
                        ON();
                        timer0_disable();

                        mode = MOD_LOG;
                        P2OUT |= BIT2;     // lcd on
                    }

                    if(mode == MOD_LOG)
                    {
                        if (*log_size_addr == 0)
                        {
                            clear_display();
                            show(text_log5);
                        }
                        else
                        {
                            if(log_page == 0)
                            {
                                log_page = 1;

                                clear_display();
                                make_text_log();
                                show(text_log3);
                                nextline();
                                show(text_log4);
                            }

                            else if (log_page == 1)
                            {
                                log_page = 0;

                                clear_display();
                                make_text_log();
                                show(text_log1);
                                nextline();
                                show(text_log2);
                            }
                        }
                    }
                }
            }

            ////////////// lpm 3.5
            else if (!powerbutton)
                __bis_SR_register(LPM3_bits + GIE);
        }
}

#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
    switch (__even_in_range(UCB0IV, USCI_I2C_UCBIT9IFG))
    {
    case 0:
        break;                           // Vector  0: No interrupts
    case 2:
        break;                           // Vector  2: ALIFG
    case 4:
        break;                           // Vector  4: NACKIFG
    case USCI_I2C_UCRXIFG0:                                  // Vector 10: RXIFG
        *PRxData++ = UCB0RXBUF;                 // RX버퍼에 저장된 값을 RX데이터에 대입
        RXByteCtr++;

        break;
    case USCI_I2C_UCTXIFG0:                                  // Vector 12: TXIFG
        if (TXByteCtr)                          // TX byte 카운터가 1이면(sendcmd)
        {
            UCB0TXBUF = TXData;                   // TX buffer에 보낼 값 저장
            TXByteCtr--;                          // TX byte 카운터 초기화
        }
        else
            UCB0CTLW0 |= UCTXSTP;                  // I2C stop
        break;
    default:
        break;
    }
}
