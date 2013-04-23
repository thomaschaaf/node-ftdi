#ifndef FTDI_CONSTANTS_H
#define FTDI_CONSTANTS_H

#define DEVICE_DESCRIPTION_TAG		"description"
#define DEVICE_SERIAL_NR_TAG		"serialNumber"
#define DEVICE_LOCATION_ID_TAG		"locationId"
#define DEVICE_VENDOR_ID_TAG		"vendorId"
#define DEVICE_PRODUCT_ID_TAG		"productId"
#define DEVICE_INDEX_TAG			"index"

#define CONNECTION_BAUDRATE_TAG		"baudrate"
#define CONNECTION_DATABITS_TAG		"databits"
#define CONNECTION_STOPBITS_TAG		"stopbits"
#define CONNECTION_PARITY_TAG		"parity"

#define CONNECTION_PARITY_NONE		"none"
#define CONNECTION_PARITY_ODD		"odd"
#define CONNECTION_PARITY_EVEN		"even"

#define FT_STATUS_0					"FT_OK"
#define FT_STATUS_1					"FT_INVALID_HANDLE"
#define FT_STATUS_2					"FT_DEVICE_NOT_FOUND"
#define FT_STATUS_3					"FT_DEVICE_NOT_OPENED"
#define FT_STATUS_4					"FT_IO_ERROR"
#define FT_STATUS_5					"FT_INSUFFICIENT_RESOURCES"
#define FT_STATUS_6					"FT_INVALID_PARAMETER"
#define FT_STATUS_7					"FT_INVALID_BAUD_RATE"
#define FT_STATUS_8					"FT_DEVICE_NOT_OPENED_FOR_ERASE"
#define FT_STATUS_9					"FT_DEVICE_NOT_OPENED_FOR_WRITE"
#define FT_STATUS_10				"FT_FAILED_TO_WRITE_DEVICE"
#define FT_STATUS_11				"FT_EEPROM_READ_FAILED"
#define FT_STATUS_12				"FT_EEPROM_WRITE_FAILED"
#define FT_STATUS_13				"FT_EEPROM_ERASE_FAILED"
#define FT_STATUS_14				"FT_EEPROM_NOT_PRESENT"
#define FT_STATUS_15				"FT_EEPROM_NOT_PROGRAMMED"
#define FT_STATUS_16				"FT_INVALID_ARGS"
#define FT_STATUS_17				"FT_NOT_SUPPORTED"
#define FT_STATUS_18				"FT_OTHER_ERROR"

#define FT_STATUS_CUSTOM_ALREADY_OPEN				"Device Already open"
#define FT_STATUS_CUSTOM_ALREADY_CLOSING			"Device Already closing"

// Lock for Library Calls
extern uv_mutex_t libraryMutex;
extern uv_mutex_t vidPidMutex;

static const char * error_strings[] = {
	FT_STATUS_0,
	FT_STATUS_1,
	FT_STATUS_2,
	FT_STATUS_3,
	FT_STATUS_4,
	FT_STATUS_5,
	FT_STATUS_6,
	FT_STATUS_7,
	FT_STATUS_8,
	FT_STATUS_9,
	FT_STATUS_10,
	FT_STATUS_11,
	FT_STATUS_12,
	FT_STATUS_13,
	FT_STATUS_14,
	FT_STATUS_15,
	FT_STATUS_16,
	FT_STATUS_17,
	FT_STATUS_18
};

inline const char* GetStatusString(int status)
{
 	return error_strings[status];   
}

#endif