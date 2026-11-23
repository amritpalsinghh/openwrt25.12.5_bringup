// i2c_uC_comm.h -- (c)2020-2021 II-VI Incorporated

/*
* device_write: Write [nLength] bytes to the gauge starting at [nRegister].
* i2c: handle to communications adapater
* nRegister: first register (=standard command) to write to
* pData: pointer to a data buffer
* nDataLength: number of data bytes
* return value: number of bytes written (0 if error)
*/
extern int device_write(mraa_i2c_context i2c, unsigned char nDevice, unsigned char nRegister, unsigned char *pData, unsigned char nDataLength, unsigned char *sequenceID);
/*
* device_read: read bytes from gauge (must be implemented for a specific system)
* i2c: handle to communications adapater
* request: request frame
* requestLength: request frame length
* out_payload: pointer to a data buffer; output
* sequenceID : sequence ID
* return value: number of bytes read (0 if error)
*/
extern int device_read(mraa_i2c_context i2c, unsigned char *request,size_t requestLength,unsigned char *out_payload, unsigned char *sequenceID);
