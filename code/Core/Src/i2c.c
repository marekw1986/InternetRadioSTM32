#include <stdio.h>
#include "i2c.h"

#define FIXLAT LATDbits.LATD2
#define FIXTRIS TRISDbits.TRISD2

//int actualClock;

//void i2c_init (void) {
//    TRISFbits.TRISF8 = 0; //SCL
//    TRISFbits.TRISF2 = 0; //SDA
//    ODCFbits.ODCF8 = 1;
//    ODCFbits.ODCF2 = 1;
//    
//    //OpenI2C1( I2C_EN, BRG_VAL );
//     I2CConfigure(I2C1A, I2C_ENABLE_HIGH_SPEED);
//     actualClock = I2CSetFrequency(I2C1A, GetPeripheralClock(), 400000);
//     if ( abs(actualClock-400000) > 400000/10 ) {
//         printf("i2c_init: I2C1A clock frequency (%u) error exceeds 10%%.\n", (unsigned)actualClock);
//     }
//     I2CEnable(I2C1A, TRUE);
//}


uint8_t i2c_rcv_byte (uint8_t address) {
//    int tmpbrg;
    int rcv = 0;
//
//    tmpbrg = I2C1ABRG;
//    I2C1ACONbits.ON = 0;
//    FIXLAT = 0;
//    FIXTRIS = 0;
//    I2C1ACONbits.ON = 1;
//    CORETIMER_DelayUs(10);
//    FIXTRIS = 1;
//    CORETIMER_DelayUs(10);
//    I2C1ABRG = tmpbrg;
//
//
//	I2CStart(I2C1A);				//Send line start condition
//    while (!(I2CGetStatus(I2C1A) & I2C_START)) {}
//	I2CSendByte(I2C1A, (address << 1) | 1);	//Write out slave address OR 1 (read command)
//    while(!I2CTransmissionHasCompleted(I2C1A)) {}
//    if(I2CByteWasAcknowledged(I2C1A) && I2CReceiverEnable(I2C1A, TRUE) != I2C_RECEIVE_OVERFLOW) {
//        while(!I2CReceivedDataIsAvailable(I2C1A));
//        rcv = I2CGetByte(I2C1A);		//Read in a value
//        I2CAcknowledgeByte (I2C1A, FALSE);
//        while( !I2CAcknowledgeHasCompleted(I2C1A));
//    }
//    else {
//        rcv = 0xFF;
//    }
//	I2CStop(I2C1A);				//Send line stop condition
	return rcv;				//Return read value
}


void i2c_send_byte (uint8_t data, uint8_t address) {	  
//    int tmpbrg;
//
//    tmpbrg = I2C1ABRG;
//    I2C1ACONbits.ON = 0;
//    FIXLAT = 0;
//    FIXTRIS = 0;
//    I2C1ACONbits.ON = 1;
//    CORETIMER_DelayUs(10);
//    FIXTRIS = 1;
//    CORETIMER_DelayUs(10);
//    I2C1ABRG = tmpbrg;
//
//    I2CStart(I2C1A);        //Send the Start Bit
//    while (!(I2CGetStatus(I2C1A) & I2C_START)) {}
//	I2CSendByte(I2C1A, (address << 1) | 0);  //Sends the slave address over the I2C line.  This must happen first so the proper slave is selected to receive data.
//    while(!I2CTransmissionHasCompleted(I2C1A)) {}
//	I2CSendByte(I2C1A, data);  //Sends data byte over I2C line
//    while(!I2CTransmissionHasCompleted(I2C1A)) {}
//	I2CStop(I2C1A);	        //Send the Stop condition
}

