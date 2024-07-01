/*
 * memory.h
 *
 *  Created on: 2024. 5. 10.
 *      Author: 82105
 */

#ifndef MEMORY_H_
#define MEMORY_H_

#include "msp430fr5949.h"
#include "sensor.h"

// 로그 데이터 저장을 위한 구조체 정의
typedef struct Divelog
{
    unsigned char diveMin;      // 다이빙한 시간 (분) 기록용
    unsigned char diveSec;      // 다이빙한 시간 (초) 기록용
    unsigned char num;          // 다이빙 로그
    unsigned short year;          // 연
    unsigned char date;          // 월 일
    unsigned char startTime;     // 시작 시간
    int tmp_avg;                // 평균 수온
    int tmp_min;                // 최소 수온
    unsigned int depth_avg;     // 평균 수심
    unsigned int depth_max;     // 최대 수심
} Divelog;

// 모드 설정을 위한 전역 변수
#define MOD_OFF 0       // 평소 저전력 대기 모드
#define MOD_WATER 1     //1번 버튼이 눌리고 다이빙 세팅 모드
#define MOD_GOING 2     // 다이빙 모드
#define MOD_LOG 3       // 로그 열람 모드
#define MAX_LOG 30      // 최대 로그 수
volatile unsigned int mode = MOD_OFF;             // 물 밖에서의 모드

#define WATER_WAIT 0    // 다이빙 대기
#define WATER_START 1   // 다이빙 시작
#define WATER_GOING 2   // 다이빙 중
#define WATER_FINISH 3  // 다이빙 종료
volatile unsigned char mode_water = WATER_WAIT;     // 물 속에서의 모드

volatile unsigned int log_page = 0;     // 로그 열람 page
volatile unsigned char log_num = 0;     // 로그 번호

// 다이빙 시간 측정
volatile unsigned char minute_water = 0;        // 다이빙 분 (측정)
volatile unsigned char second_water = 0;        // 다이빙 초 (측정)
volatile unsigned long diving_sec = 0;          // 시간 측정용 변수

// 다이빙 시 센서 값
volatile unsigned int depth_before = 0;
volatile unsigned int tmp_avg = 0;
volatile unsigned int depth_avg = 0;
volatile float tmp_avg_f = 0.0f;
volatile float depth_avg_f = 0.0f;
volatile unsigned int tmp_min = 999;
volatile unsigned int depth_max = 0;

// memory map
Divelog* log_addr = (Divelog*)  0x19FF;  // 1800 ~ 19DF

unsigned int* firstOn = (unsigned int*) 0x197F;
unsigned int* water_startTime = (unsigned int*) 0x18FF;
unsigned int* log_size_addr = (unsigned int*) 0x187F;

// function prototype
inline void power_init();               // 초기 설정
inline void set_time();                 // 구동 시작 시간 설정
void insert_log();                      // 로그 기록
void delete_log(unsigned char num);     // 로그 삭제

inline void power_init()
{
    CSCTL0_H = CSKEY >> 8;                  // ctl 레지스터 unlock (수정 가능하게끔)
    CSCTL1 |= DCOFSEL_0;     // DCO 1MHz 사용, LOW speed.

    CSCTL2 = SELM__DCOCLK | SELS__DCOCLK;

    CSCTL3 |= (DIVS__4 | DIVM__4);    // clk divider : 4 : 250kHz
    CSCTL0_H = 0;                     // Lock CS registers

    WDTCTL |= (WDTPW | WDTHOLD);   // stop watchdog timer
    PMMCTL0_H = PMMPW_H;        // PMM 레지스터 수정 접근
    PM5CTL0 &= ~LOCKLPM5;       // lpmx.5에서 I/O unlock

    *firstOn = 1;
    if (*firstOn == 1)
        {
            set_time();
            *firstOn = 0;
        }

    __enable_interrupt();

    // BUZZER OUTPUT
    PJSEL0 &= ~BIT4;
    PJSEL1 &= ~BIT4; // PJ.4를 GPIO로 설정
    PJDIR |= BIT4;
    PJOUT &= ~BIT4;

    // 부저 알림
    PJOUT |= BIT4;
    __delay_cycles(10000);
    PJOUT &= ~BIT4;

    // BACKLIGHT OUTPUT
    P2SEL0 &= ~BIT1;
    P2SEL1 &= ~BIT1;
    P2DIR |= BIT1;

    // backlight 알림
    P2OUT |= BIT1;
    __delay_cycles(10000);
    P2OUT &= ~BIT1;

    // LCD POWER OUTPUT
    P2SEL0 &= ~BIT2;
    P2SEL1 &= ~BIT2;
    P2DIR |= BIT2;

    // SENSOR POWER OUTPUT
    P2SEL0 &= ~BIT6;
    P2SEL1 &= ~BIT6;
    P2DIR |= BIT6;
}

inline void set_time()      // 구동 시작 시간 설정
{
    RTCCTL1 |= RTCHOLD;        // RTC 잠시 멈춤

    RTCYEAR = 2024;
    RTCMON = 6;
    RTCDAY = 5;
    RTCDOW = 0;

    RTCHOUR = 19;
    RTCMIN = 00;
    RTCSEC = 0;                 // 날짜 초기 세팅

    RTCCTL0 &= ~RTCTEVIFG;       // Clear RTC time event interrupt flag

    RTCCTL1 &= ~RTCHOLD;       // RTC 다시 가동
}

void insert_log()       //  로그 저장
{
    if (*log_size_addr == MAX_LOG)      // 로그가 꽉 찼을 경우 첫번째 로그 삭제
        delete_log((unsigned char) 0);

    Divelog* list = log_addr + *log_size_addr;

    // 로그 데이터 입력
    list->diveMin = diving_sec / 60;        // 다이빙 분
    list->diveSec = diving_sec % 60;        // 다이빙 초
    list->year = RTCYEAR;                   // 연
    list->date = (RTCMON / 10) * 1000 + (RTCMON % 10) * 100 + (RTCDAY / 10) * 10
            + (RTCDAY % 10);                // MMDD 형식으로 표현
    list->startTime = *water_startTime;     //
    list->tmp_avg = tmp_avg;                // 평균 수온
    list->tmp_min = tmp_min;                // 최저 수온
    list->depth_avg = depth_avg;            // 평균 수심
    list->depth_max = depth_max;            // 최대 수심

    // increase size
    (*log_size_addr)++;
}

void delete_log(unsigned char num)      // 로그 삭제
{
    if (MAX_LOG <= num)     // 해당 로그가 최대 저장 가능 로그보다 크면 취소
        return;

    unsigned int log_size = *log_size_addr;
    Divelog* list = log_addr;
    int i;

    for (i = num; i < log_size - 1; i++)        // 데이터 밀기
    {
        (list + i)->diveMin = (list + i + 1)->diveMin;
        (list + i)->diveSec = (list + i + 1)->diveSec;
        (list + i)->year = (list + i + 1)->year;
        (list + i)->date = (list + i + 1)->date;
        (list + i)->startTime = (list + i + 1)->startTime;
        (list + i)->tmp_avg = (list + i + 1)->tmp_avg;
        (list + i)->tmp_min = (list + i + 1)->tmp_min;
        (list + i)->depth_avg = (list + i + 1)->depth_avg;
        (list + i)->depth_max = (list + i + 1)->depth_max;
    }

    // decrease size
    *log_size_addr = log_size - 1;
}

#endif /* MEMORY_H_ */
