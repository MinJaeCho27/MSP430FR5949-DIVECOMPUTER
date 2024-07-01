/*
 * lcd.h
 *
 *  Created on: 2024. 5. 10.
 *      Author: 82105
 */

#ifndef LCD_H_
#define LCD_H_

#include "msp430fr5949.h"
#include "memory.h"
#include <stdlib.h>

// I2C 통신 내 세부 신호 high, low
inline void data_high();        // data high
inline void data_low();         // data low
inline void clk_high();         // clk high
inline void clk_low();          // clk low
inline void reset_high();       // reset high
inline void reset_low();        // reset low

inline void I2C_Start();
inline void I2C_Stop();
inline void I2C_out(unsigned char j);
inline void lcd_init();

inline void show(unsigned char *text);
inline void nextline(void);
inline void clear_display(void);

inline void make_text_main();

inline void make_text_water();
inline void make_text_water1();
inline void make_text_water2();

inline void make_text_log();
inline void make_text_log1(unsigned char num);
inline void make_text_log2(unsigned char num);
inline void make_text_log3(unsigned char num);
inline void make_text_log4(unsigned char num);

unsigned char* itoc3(unsigned int num);
unsigned char* itoc4(unsigned int num);

inline void make_sample_data();

const char Slave = 0x7C;
const char Comsend = 0x00;
const char Datasend = 0x40;
const char Line2 = 0xC0;
volatile unsigned int countdown = 180; // 3분 (180초)

// 화면 출력 format
unsigned char text_main1[] = { "YYYY/MM/DD hh:mm" };    // 다이빙 대기 화면 1줄
unsigned char text_main2[] = { "READY  TO  DIVE " };   // 다이빙 대기 화면 2줄
unsigned char text_water1[] = { "TI mm:ssDE DP.Tm" };   // 다이빙 화면 1줄
unsigned char text_water2[] = { "TP tm.pCDEC 3:00"};    // 다이빙 화면 2줄
unsigned char text_log1[] = { "NNN  YYYY/MM/DD" };      // 로그 1화면 1줄
unsigned char text_log2[] = { "hh:mm  TIME)mm:ss" };    // 로그 1화면 2줄
unsigned char text_log3[] = { "A)tm.pC mT)tm.pC" };     // 로그 2화면 1줄
unsigned char text_log4[] = { "A)DP.Tm MD)DP.Tm" };     // 로그 2화면 2줄
unsigned char text_log5[] = { "NO LOG AVAILABLE" };     // 로그 없을 때 화면

inline void data_high()
{
    P1OUT |= BIT6;      // SDA : P1.6 high
}

inline void data_low()
{
    P1OUT &= ~BIT6;    // SDA LOW
}

inline void clk_high()
{
    P1OUT |= BIT7;      // SCL HIGH
}

inline void clk_low()
{
    P1OUT &= ~BIT7;      // SCL LOW
}

inline void reset_high()
{
    P3OUT |= BIT6;
}

inline void reset_low()
{
    P3OUT &= ~BIT6;
}

inline void I2C_Start()     // I2C 시작 신호
{
    clk_high();
    data_high();
    data_low();
    clk_low();
}

inline void I2C_Stop()      // I2C 정지 신호
{
    data_low();
    clk_low();
    clk_high();
    data_high();

}

inline void I2C_out(unsigned char j)    // 8bit 명령어 혹은 데이터를 출력
{
    int n;
    unsigned char d = j;
    for (n=0 ; n<8 ; n++)
    {
        if ((d & 0x80) == 0x80)
            data_high();
        else
            data_low();
        d <<= 1;
        clk_low();
        clk_high();
        clk_low();
    }
    clk_high();
    while((P1OUT & BIT6) == 1)
    {
        clk_low();
        clk_high();
    }
    clk_low();
}

inline void lcd_init()
{
    P1DIR |= BIT6;
    P1DIR |= BIT7;
    P3DIR |= BIT6;      // 리셋

    P2DIR |= BIT2;      // OUTPUT
    P2OUT |= BIT2;      // lcd 전원 공급

    // reset
    reset_low();
    __delay_cycles(20000); // delay 20
    reset_high();

    // init function
    __delay_cycles(50000); // delay 50
    I2C_Start();
    I2C_out(Slave);
    I2C_out(Comsend);
    I2C_out(0x38); // 8bit, 2줄 출력, is=0(일반 모드)
    __delay_cycles(10);
    I2C_out(0x39); // 8bit, 2줄 출력, is=1(확장 모드)
    __delay_cycles(10);
    I2C_out(0x14); // Bias(명암) 1/5 : 전력 소모 낮추도록 동작
    I2C_out(0x78); // Contrast(명암 정도) :0111/1000
    I2C_out(0x5C); // Icon display x, booster on, contrast C5/C4
    I2C_out(0x6F); // Follower circuit on, amplifier on
    I2C_out(0x0C); // display on, 커서 사용 x
    I2C_out(0x01); // Clear display
    I2C_out(0x06);
    __delay_cycles(10);
    I2C_Stop();
}

inline void show(unsigned char *text)       // lcd에 출력
{
    int n;

    I2C_Start();
    I2C_out(0x7C); // lcd의 slave 주소
    I2C_out(Datasend); // data를 send

    for (n = 0; n < 16; n++)
    {
        I2C_out(*text);
        ++text;
    }

    I2C_Stop();
}

/* Move to Line 2 */
inline void nextline(void)
{
    I2C_Start();
    I2C_out(0x7C);
    I2C_out(Comsend);
    I2C_out(Line2);
    I2C_Stop();
}

inline void clear_display(void)     // lcd 클리어
{
    I2C_Start();
    I2C_out(Slave);
    I2C_out(Comsend); // Control byte: all following bytes are commands.
    __delay_cycles(10);
    I2C_out(0x01); // Clear display.
    __delay_cycles(27);
    I2C_Stop();
}

inline void make_text_main()        // 대기 화면 build
{
    unsigned int rtcyear = RTCYEAR;
    text_main1[0] = (rtcyear / 1000) + '0';
    text_main1[1] = ((rtcyear % 1000) / 100) + '0';
    text_main1[2] = ((rtcyear % 100) / 10) + '0';
    text_main1[3] = (rtcyear % 10) + '0';

    text_main1[5] = (RTCMON / 10) + '0';
    text_main1[6] = (RTCMON % 10) + '0';

    text_main1[8] = (RTCDAY / 10) + '0';
    text_main1[9] = (RTCDAY % 10) + '0';

    text_main1[11] = (RTCHOUR / 10) + '0';
    text_main1[12] = (RTCHOUR % 10) + '0';

    text_main1[14] = (RTCMIN / 10) + '0';
    text_main1[15] = (RTCMIN % 10) + '0';
}

inline void make_text_water()       // 다이빙 화면 build
{
    make_text_water1();
    make_text_water2();
}

inline void make_text_water1()      // 다이빙 화면 1줄
{
    text_water1[3] = (RTCMIN / 10) + '0';
    text_water1[4] = (RTCMIN % 10) + '0';       // 분

    text_water1[6] = second_water / 10 + '0';
    text_water1[7] = second_water % 10 + '0';  // 초

    unsigned char* depth_text = itoc3(depth_sensor);

    if (depth_sensor < 10)
    {
        text_water1[11] = '0';      // 수심이 1단위
        text_water1[12] = depth_text[1];
        text_water1[14] = depth_text[2];
    }
    else if (depth_sensor == 0)
        text_water1[14] = '0';      // 수심이 0일 때 소수점 처리
    else
    {
        text_water1[11] = depth_text[0];
        text_water1[12] = depth_text[1];
        text_water1[14] = depth_text[2];
    }
}

inline void make_text_water2()  // 다이빙 화면 2줄
{
    unsigned char* tmp_text = itoc3(tmp_sensor);
    text_water2[3] = tmp_text[0];
    text_water2[4] = tmp_text[1];
    text_water2[6] = tmp_text[2];

    text_water2[12] = (countdown / 60) + '0';
    text_water2[14] = ((countdown % 60) / 10) + '0';
    text_water2[15] = ((countdown % 60) % 10) + '0';     // 데코타임 코드
}

inline void make_text_log()     // 로그 화면 build
{
    make_text_log1(log_num);
    make_text_log2(log_num);
    make_text_log3(log_num);
    make_text_log4(log_num);
}

inline void make_text_log1(unsigned char num)   // 로그 1화면 1줄
{
    Divelog* list = log_addr + num;

    // log num
    text_log1[0] = (num + 1) / 100 + '0';
    text_log1[1] = ((num + 1) % 100) / 10 + '0';
    text_log1[2] = ((num + 1) % 100) % 10 + '0';

    unsigned char* year = itoc4(list->year);
    text_log1[6] = year[0];
    text_log1[7] = year[1];
    text_log1[8] = year[2];
    text_log1[9] = year[3];

    unsigned char* date = itoc4(list->date);
    text_log1[11] = date[0];
    text_log1[12] = date[1];
    text_log1[14] = date[2];
    text_log1[15] = date[3];
}

inline void make_text_log2(unsigned char num)   // 로그 1화면 2줄
{
    Divelog* list = log_addr + num;

    unsigned char* startTime = itoc4(list->startTime);
    text_log2[0] = startTime[0];
    text_log2[1] = startTime[1];
    text_log2[3] = startTime[2];
    text_log2[4] = startTime[3];

    unsigned char* divemin = itoc3((unsigned int) list->diveMin);
    text_log2[11] = divemin[0];
    text_log2[12] = divemin[1];

    unsigned char* divesec = itoc3((unsigned int) list->diveSec);
    text_log2[14] = divesec[0];
    text_log2[15] = divesec[1];
}

inline void make_text_log3(unsigned char num)   // 로그 2화면 1줄
{
    Divelog* list = log_addr + num;

    unsigned char* tmp_avg = itoc3(list->tmp_avg);
    text_log3[2] = tmp_avg[0];
    text_log3[3] = tmp_avg[1];
    text_log3[5] = tmp_avg[2];

    unsigned char* tmp_min = itoc3(list->tmp_min);
    text_log3[11] = tmp_min[0];
    text_log3[12] = tmp_min[1];
    text_log3[14] = tmp_min[2];
}

inline void make_text_log4(unsigned char num)   // 로그 2화면 2줄
{
    Divelog* list = log_addr + num;

    unsigned char* depth_avg = itoc3(list->depth_avg);
    text_log4[2] = depth_avg[0];
    text_log4[3] = depth_avg[1];
    text_log4[5] = depth_avg[2];

    unsigned char* depth_max = itoc3(list->depth_max);
    text_log4[11] = depth_max[0];
    text_log4[12] = depth_max[1];
    text_log4[14] = depth_max[2];
}

unsigned char* itoc3(unsigned int num)
{
    static unsigned char c[] = "123";
    c[0] = (num / 100) + '0';
    if (c[0] == '0')
        c[0] = ' ';
    num %= 100;
    c[1] = (num / 10) + '0';
    c[2] = (num % 10) + '0';

    return c;
}

unsigned char* itoc4(unsigned int num)
{
    static unsigned char c[] = "1234";
    c[0] = (num / 1000) + '0';
    num %= 1000;
    c[1] = (num / 100) + '0';
    num %= 100;
    c[2] = (num / 10) + '0';
    c[3] = (num % 10) + '0';

    return c;
}

inline void make_sample_data()      // 샘플 데이터 만들기
{
    unsigned int i;
    Divelog* list = log_addr;
    for (i = 0; i < MAX_LOG; i++)
    {
        (list + i)->diveMin = (unsigned char) 0;
        (list + i)->diveSec = (unsigned char) 0;
        (list + i)->year = 0;
        (list + i)->date = 0;
        (list + i)->startTime = 0;
        (list + i)->tmp_avg = 0;
        (list + i)->tmp_min = 0;
        (list + i)->depth_avg = 0;
        (list + i)->depth_max = 0;
    }
    *log_size_addr = 0;

    for (i = 0; i < MAX_LOG; i++)
        insert_log();
}

#endif /* LCD_H_ */
