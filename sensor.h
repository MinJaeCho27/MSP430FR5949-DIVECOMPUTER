/*
 * sensor.h
 *
 *  Created on: 2024. 5. 10.
 *      Author: 82105
 */

#ifndef SENSOR_H_
#define SENSOR_H_

#include "msp430fr5949.h"

// 센서 커맨드
#define RD_PROM 0xA0
#define RRESET   0x1E
#define CONV_P  0x44
#define CONV_T  0x50
#define READ    0x00

unsigned char RXData;
unsigned char TXData;
unsigned char TXByteCtr;

unsigned char *PRxData;                     // Pointer to RX data
unsigned char RXByteCtr;
volatile unsigned char RxBuffer[128];       // Allocate 128 byte of RAM

volatile unsigned int tmp_sensor = 0;
volatile unsigned int depth_sensor = 0;

unsigned int coefficient[8];       // Coefficients;
unsigned long D1;          // 측정 압력 (D1)
unsigned long D2;          // 측정 온도 (D2)

inline void read_prom();
inline void send_cmd(unsigned char data);
inline void recv_data2();
inline void recv_data3();
void calc_data();
inline void sensor_enable();
void sensor_init();

void calc_data()        // 온도 계산 함수
{
    long TEMP;
    long dT;

    long long Ti, OFFi, SENSi, OFF, SENS, OFF2, SENS2; // working variables
    long long TEMP2, P2;

    dT = D2 - ((long) coefficient[5] << 8);
    TEMP = 2000 + (((long long) dT * coefficient[6]) >> 23);
    OFF = ((long long) coefficient[2] << 16)
            + (((coefficient[4] * (long long) dT)) >> 7);
    SENS = ((long long) coefficient[1] << 15)
            + (((coefficient[3] * (long long) dT)) >> 8);

    /**********온도에 따른 보정 ********/

    if (TEMP < 2000)       // 측정 온도가 20도 미만일 때
    {
        Ti = 3 * (((long long) dT * dT) >> 33);
        OFFi = 3 * ((TEMP - 2000) * (TEMP - 2000)) >> 1;
        SENSi = 5 * ((TEMP - 2000) * (TEMP - 2000)) >> 3;

        if (TEMP < -1500)      // 영하 -15도 미만일 때
        {
            OFFi = OFFi + 7 * ((TEMP + 1500) * (TEMP + 1500));
            SENSi = SENSi + 4 * ((TEMP + 1500) * (TEMP + 1500));
        }
    }

    else        // 20도 이상일 때
    {
        Ti = 2 * (((unsigned long long) dT * dT) >> 37);
        OFFi = ((TEMP - 2000) * (TEMP - 2000)) >> 4;
        SENSi = 0;
    }

    OFF2 = OFF - OFFi;
    SENS2 = SENS - SENSi;

    TEMP2 = (TEMP - Ti) / 10 ;      // 10 * 결과 온도(섭씨)
    P2 = ((((D1 * SENS2) >> 21) - OFF2) >> 13) / 10;  // mbar 단위

    depth_sensor = (unsigned int) ((float) P2 / 101.325f);     // 수심(m) 값 (* 10)

    tmp_sensor = (unsigned int) TEMP2;     // 결과 온도(* 10)
}

// prom에 저장된 데이터 읽어오는 함수
inline void read_prom()
{
    unsigned char j;
    for (j = 0; j <= 7; j++)    // 0~7bit까지 prom의 값을 읽어옴
    {
        send_cmd(0xA0 + (j * 2));
        __delay_cycles(100);
        recv_data2();
        __delay_cycles(3000);
        coefficient[j] = (RxBuffer[0] << 8) | RxBuffer[1];
    }
}

inline void send_cmd(unsigned char data)        // 센서 컨트롤 커맨드 전송
{
    TXData = data;          // 보낼 커맨드를 TXData에 저장
    TXByteCtr = 1;          // 카운터 1로 초기화
    while (UCB0CTLW0 & UCTXSTP)     // I2C 시작이 완료될때까지 대기
    {}

    UCB0CTLW0 |= UCTR + UCTXSTT;
    __no_operation();
    __enable_interrupt();           // 인터럽트 허용
}

inline void recv_data2()
{
    UCB0TBCNT = 2;      // I2C 수신 바이트 2으로 설정

    PRxData = (unsigned char *) RxBuffer;       // 수신 데이터 시작 위치 설정
    RXByteCtr = 1;      // 수신한 바이트 수 1바이트

    while (UCB0CTLW0 & UCTXSTP)     // 수신 준비될때까지 대기
    {
    }

    UCB0CTLW0 &= ~UCTR;             // I2C 수신 모드로 설정
    UCB0CTLW0 |= UCTXSTT;           // I2C 수신 시작 신호
}

// 3바이트 데이터 수신 함수
inline void recv_data3()
{
    UCB0TBCNT = 3;      // I2C 수신 바이트 3으로 설정

    PRxData = (unsigned char *) RxBuffer;       // 수신 데이터 시작 위치 설정
    RXByteCtr = 1;      // 수신한 바이트 수 1바이트

    while (UCB0CTLW0 & UCTXSTP)     // 수신 준비될때까지 대기
    {
    }

    UCB0CTLW0 &= ~UCTR;             // I2C 수신 모드로 설정
    UCB0CTLW0 |= UCTXSTT;           // I2C 수신 시작 신호
}

// 센서로 값 받는 함수
inline void sensor_enable()
{
    send_cmd(RRESET);
    __delay_cycles(4000);
    read_prom();                // prom에 있는 데이터 받아옴
    __delay_cycles(3000);

    send_cmd(CONV_T);           // 온도 변환 명령 전송
    __delay_cycles(3000);
    send_cmd(READ);             // 데이터 읽기
    __delay_cycles(100);
    recv_data3();               // 센서에서 데이터 수신
    __delay_cycles(3000);
    D2 = ((unsigned long) RxBuffer[0] << 16)    // 수신 온도값 저장
            + ((unsigned long) RxBuffer[1] << 8) + RxBuffer[2];

    send_cmd(CONV_P);           // 압력 변환 명령 전송
    __delay_cycles(3000);
    send_cmd(READ);             // 데이터 읽기
    __delay_cycles(1000);
    recv_data3();               // 데이터 수신
    __delay_cycles(3000);
    D1 = ((unsigned long) RxBuffer[0] << 16)    // 수신 압력값 저장
            + ((unsigned long) RxBuffer[1] << 8) + RxBuffer[2];

    calc_data();
}


#endif /* SENSOR_H_ */
