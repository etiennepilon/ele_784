/*
 *  * File         : ele784-lab1.c
 *  * Description  : ELE784 Lab1 source
 *  *
 *  * Etudiants:  JORA09019100 (Alexandru Jora)
 *  *             MUKM28098503 (Mukandila Mukandila)
 *  */

#define CHARDRIVER_IOC_MAGIC        'e'
#define CHARDRIVER_IOCRESET         _IO(CHARDRIVER_IOC_MAGIC,0)
#define CHARDRIVER_GETNUMDATA       _IOR(CHARDRIVER_IOC_MAGIC, 1, int)
#define CHARDRIVER_GETNUMREADER     _IOR(CHARDRIVER_IOC_MAGIC, 2, int)
#define CHARDRIVER_GETBUFSIZE       _IOR(CHARDRIVER_IOC_MAGIC, 3, int)
#define CHARDRIVER_SETBUFSIZE       _IOW(CHARDRIVER_IOC_MAGIC, 4, int)
#define CHARDRIVER_GETMAGICNUMBER   _IOR(CHARDRIVER_IOC_MAGIC, 5, char)
#define CHARDRIVER_IOC_MAXNR        5
