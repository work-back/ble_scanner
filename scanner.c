// To compile this program, you need to install:
//   sudo apt-get install libbluetooth-dev
// Then you can compile it with:
//   cc scanner.c -lbluetooth -o scanner
// You can then run it with:
//   ./scanner

// Copyright (c) 2021 David G. Young
// Copyright (c) 2015 Damian Kołakowski. All rights reserved.
// License: BSD 3.  See: https://github.com/davidgyoung/ble-scanner

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <time.h>
#include <arpa/inet.h>

#include "bleapi.h"

#define NS_STREAM_TO_UINT8(u8, p)   {u8 = (uint8_t)(*(p)); (p) += 1;}
#define NS_BTM_BLE_CACHE_ADV_DATA_MAX      62

uint8_t *BTM_CheckAdvData( uint8_t *p_adv, uint8_t type, uint8_t *p_length)
{
    uint8_t *p = p_adv;
    uint8_t length;
    uint8_t adv_type;

    NS_STREAM_TO_UINT8(length, p);

    while ( length && (p - p_adv < NS_BTM_BLE_CACHE_ADV_DATA_MAX)) {
        NS_STREAM_TO_UINT8(adv_type, p);

        if ( adv_type == type ) {
            /* length doesn't include itself */
            *p_length = length - 1; /* minus the length of type */
            return p;
        }

        p += length - 1; /* skip the length of data */

        /* Break loop if advertising data is in an incorrect format,
           as it may lead to memory overflow */
        if (p >= p_adv + NS_BTM_BLE_CACHE_ADV_DATA_MAX) {
            break;
        }

        NS_STREAM_TO_UINT8(length, p);
    }

    *p_length = 0;
    return NULL;
}

uint8_t *esp_ble_resolve_adv_data( uint8_t *adv_data, uint8_t type, uint8_t *length)
{
    if (((type < ESP_BLE_AD_TYPE_FLAG) || (type > ESP_BLE_AD_TYPE_128SERVICE_DATA)) &&
            (type != ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE)) {
        printf("the eir type not define, type = %x\n", type);
        return NULL;
    }

    if (adv_data == NULL) {
        printf("Invalid p_eir data.\n");
        return NULL;
    }

    return (BTM_CheckAdvData( adv_data, type, length));
}

void __dump_data(const unsigned char *ptr, int len, const char *func, int line)
{
    int i;
    int _b_len = 0;
    char _buf[1024 * 4];

#define __PRINTF2BUF(fmt, ...) \
    _b_len += snprintf(((char *)_buf) + _b_len, sizeof(_buf) -_b_len, fmt, ##__VA_ARGS__)

    __PRINTF2BUF("\n=============================================\n");
    __PRINTF2BUF("[%s][%d]", func, line);

    for (i = 0; i < len; i++) {
        if (!(i%16)) {
            __PRINTF2BUF("\n %04x", i);
        }
        __PRINTF2BUF(" %02x", ptr[i]);
    }
    __PRINTF2BUF("\n=============================================\n");

    printf("%s\n", _buf);

    return;
}

#define NS_dump_data(ptr, len) __dump_data((unsigned char *)ptr, len, __func__, __LINE__)

void bt_dump_all_ext_type(uint8_t *p_adv)
{
    uint8_t *p = p_adv;
    uint8_t length;
    uint8_t adv_type;

    NS_STREAM_TO_UINT8(length, p);

    while ( length && (p - p_adv < NS_BTM_BLE_CACHE_ADV_DATA_MAX)) {
        NS_STREAM_TO_UINT8(adv_type, p);

        // if ( adv_type == type ) {
        //     /* length doesn't include itself */
        //     *p_length = length - 1; /* minus the length of type */
        //     return p;
        // }

        printf("[%s][%d] LYJ@NS --------> adv_type:%d\n", __func__, __LINE__, adv_type);
        NS_dump_data(p, length - 1);

        p += length - 1; /* skip the length of data */

        /* Break loop if advertising data is in an incorrect format,
           as it may lead to memory overflow */
        if (p >= p_adv + NS_BTM_BLE_CACHE_ADV_DATA_MAX) {
            break;
        }

        NS_STREAM_TO_UINT8(length, p);
    }

    return;
}

void parse_ext_pdu(uint8_t *data, int length) {
    uint8_t pdu_type = data[0] & 0x0F;
    uint8_t pdu_length = (data[0] & 0x10) ? data[1] : 0;
    printf("PDU Type: %d\n", pdu_type);
    printf("Length: %d\n", pdu_length);
    printf("Data: ");
    for (int i = 2; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}


struct hci_request ble_hci_request(uint16_t ocf, int clen, void * status, void * cparam)
{
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = ocf;
	rq.cparam = cparam;
	rq.clen = clen;
	rq.rparam = status;
	rq.rlen = 1;
	return rq;
}

int handle_ble_adv_rpt_0(evt_le_meta_event * meta_event)
{
    le_advertising_info *info = (void *) (meta_event->data + 1);
    uint8_t *data = info->data;
    int data_length = info->length;

    //printf("evt_type:%d\n", info->evt_type);
    //bt_dump_all_ext_type(info->data);
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    char data_buf[256] = {0};
    adv_name = esp_ble_resolve_adv_data(info->data, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
    if (adv_name_len + 1 > sizeof(data_buf)) {
        return -1;
    }

    memcpy(data_buf, adv_name, adv_name_len);

    if (adv_name_len <= 3) {
        return -1;
    }

    #if 0
    if (strncmp(data_buf, "MI 6", 4) != 0) {
        return -1;
    }
    #endif

    printf("[%s][%d] LYJ@NS -------->adv_name:%s, event_type:%08X\n", __func__, __LINE__, data_buf, info->evt_type);
    // bt_dump_all_ext_type(info->data);

    uint8_t *uuid = NULL;
    uint8_t uuid_len = 0;
    uuid = esp_ble_resolve_adv_data(info->data, ESP_BLE_AD_TYPE_128SRV_CMPL, &uuid_len);
    if (uuid && uuid_len) {
        memset(data_buf, 0, sizeof(data_buf));
        if (uuid_len + 1 < sizeof(data_buf)) {
            memcpy(data_buf, uuid, uuid_len);
            NS_dump_data(data_buf, uuid_len);
        }
    }

    return 0;
}

int handle_ble_adv_rpt_i(le_advertising_info *info)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    char data_buf[256] = {0};
    adv_name = esp_ble_resolve_adv_data(info->data, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
    if (adv_name_len + 1 > sizeof(data_buf)) {
        return -1;
    }

    memcpy(data_buf, adv_name, adv_name_len);

    if (adv_name_len <= 3) {
        return -1;
    }

    #if 0
    if (strncmp(data_buf, "MI 6", 4) != 0) {
        return -1;
    }
    #endif

    printf("[%s][%d] LYJ@NS -------->adv_name:%s, event_type:%08X\n", __func__, __LINE__, data_buf, info->evt_type);
    // bt_dump_all_ext_type(info->data);

    uint8_t *uuid = NULL;
    uint8_t uuid_len = 0;
    uuid = esp_ble_resolve_adv_data(info->data, ESP_BLE_AD_TYPE_128SRV_CMPL, &uuid_len);
    if (uuid && uuid_len) {
        memset(data_buf, 0, sizeof(data_buf));
        if (uuid_len + 1 < sizeof(data_buf)) {
            memcpy(data_buf, uuid, uuid_len);
            NS_dump_data(data_buf, uuid_len);
        }
    }

    return 0;
}

int handle_ble_adv_rpt(evt_le_meta_event * meta_event)
{
    uint8_t reports_count = meta_event->data[0];
    void * offset = meta_event->data + 1;
    le_advertising_info * info = NULL;

    while ( reports_count-- ) {
        info = (le_advertising_info *)offset;
        char addr[18];
        ba2str(&(info->bdaddr), addr);
        //printf("%s - RSSI %d\n", addr, (char)info->data[info->length]);
        offset = info->data + info->length + 2;
        handle_ble_adv_rpt_i(info);
    }

    return 0;
}

int handle_ble_scan(const char *buf, int len)
{
	evt_le_meta_event * meta_event;
	le_advertising_info * info;

    if ( len >= HCI_EVENT_HDR_SIZE ) {
        meta_event = (evt_le_meta_event*)(buf + HCI_EVENT_HDR_SIZE+1);
        // printf("[%s][%d] LYJ@NS -------->event_type:%08X\n", __func__, __LINE__, meta_event->subevent);
        if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) {
            handle_ble_adv_rpt(meta_event);
        }
    }
    return 0;
}

int main()
{
	int ret, status;

	int device = hci_open_dev(1);
	if ( device < 0 ) {
		device = hci_open_dev(0);
		if (device >= 0) {
			printf("Using hci0\n");
		}
	}
	else {
		printf("Using hci1\n");
	}

	if ( device < 0 ) {
		perror("Failed to open HCI device.");
		return 0;
	}

	// Set BLE scan parameters.

	le_set_scan_parameters_cp scan_params_cp;
	memset(&scan_params_cp, 0, sizeof(scan_params_cp));
	scan_params_cp.type 			= 0x01;
	scan_params_cp.interval 		= htobs(0x0010);
	scan_params_cp.window 			= htobs(0x0010);
	scan_params_cp.own_bdaddr_type 	= 0x00; // Public Device Address (default).
	scan_params_cp.filter 			= 0x00; // Accept all.

	struct hci_request scan_params_rq = ble_hci_request(OCF_LE_SET_SCAN_PARAMETERS, LE_SET_SCAN_PARAMETERS_CP_SIZE, &status, &scan_params_cp);

	ret = hci_send_req(device, &scan_params_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to set scan parameters data.");
		return 0;
	}

    printf("OCF_LE_SET_SCAN_PARAMETERS status:%d\n", status);

	// Set BLE events report mask.

	le_set_event_mask_cp event_mask_cp;
	memset(&event_mask_cp, 0, sizeof(le_set_event_mask_cp));
	int i = 0;
	for ( i = 0 ; i < 8 ; i++ ) event_mask_cp.mask[i] = 0xFF;

	struct hci_request set_mask_rq = ble_hci_request(OCF_LE_SET_EVENT_MASK, LE_SET_EVENT_MASK_CP_SIZE, &status, &event_mask_cp);
	ret = hci_send_req(device, &set_mask_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to set event mask.");
		return 0;
	}

    printf("OCF_LE_SET_EVENT_MASK status:%d\n", status);

	// Enable scanning.

	le_set_scan_enable_cp scan_cp;
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable 		= 0x01;	// Enable flag.
	scan_cp.filter_dup 	= 0x00; // Filtering disabled.

	struct hci_request enable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);

	ret = hci_send_req(device, &enable_adv_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to enable scan.");
		return 0;
	}

	// Get Results.

	struct hci_filter nf;
	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);
	if ( setsockopt(device, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 ) {
		hci_close_dev(device);
		perror("Could not set socket options\n");
		return 0;
	}


	uint8_t buf[HCI_MAX_EVENT_SIZE];
	int len;

	while (1) {
		len = read(device, buf, sizeof(buf));
		if ( len >= HCI_EVENT_HDR_SIZE ) {
            hci_event_hdr *hdr = (hci_event_hdr *) buf;
            // byte order
            // printf("[%s][%d] LYJ@NS -------->hci hdr type:%02X==%02X, plen:%d\n", __func__, __LINE__, hdr->plen, EVT_LE_META_EVENT, hdr->evt);
            handle_ble_scan(buf, len);
		}
	}

	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = 0x00;	// Disable flag.

	struct hci_request disable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);
	ret = hci_send_req(device, &disable_adv_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to disable scan.");
		return 0;
	}

	hci_close_dev(device);

	return 0;
}
