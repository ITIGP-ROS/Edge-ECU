#ifndef STD_TYPES_H
#define STD_TYPES_H

typedef unsigned char       uint8_t;        
typedef unsigned short int  uint16_t;        
typedef unsigned long int   uint32_t;       
typedef signed char         int8_t;        
typedef signed short int    int16_t;       
typedef signed long int     int32_t;       
typedef float               float32_t;       
typedef double              float64_t;       

typedef enum {
    STD_ERROR = 0,
    STD_SUCCESS,
    STD_TIMEOUT,
    STD_BUSY,
    STD_ACK,
    STD_NACK,
    STD_CMD_FINISHED
} STD_ReturnType;

#ifndef NULL
#define NULL    (void *)0
#endif

#endif