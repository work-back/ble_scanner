#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define OGF_LE_CTL 0x08
#define OCF_LE_SET_SCAN_PARAMETERS 0x000B
#define OCF_LE_SET_SCAN_ENABLE 0x000C
#define EVT_LE_META_EVENT 0x3E
#define EVT_LE_ADVERTISING_REPORT 0x02

// 定义LE Meta事件头
struct hci_event_hdr {
    uint8_t evt;
    uint8_t plen;
} __attribute__ ((packed));

// 定义LE Advertising Report事件
struct evt_le_advertising_info {
    uint8_t evt_type;
    uint8_t bdaddr_type;
    bdaddr_t bdaddr;
    uint8_t length;
    uint8_t data[0];
} __attribute__((packed));

// 解析扩展PDU
void parse_ext_pdu(uint8_t *data, int length) {
    // 扩展PDU数据格式
    // 第1字节：PDU类型
    // 第2字节：PDU长度
    // 第3-N字节：PDU数据
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

int main() {
    int device_id;
    struct hci_filter old_options;
    struct hci_dev_info dev_info;

    device_id = hci_get_route(NULL);
    int hci_socket = hci_open_dev(device_id);
    if (hci_socket < 0) {
        perror("HCI socket open failed");
        exit(1);
    }

    struct hci_request scan_params_rq;
    le_set_scan_parameters_cp scan_params;
    memset(&scan_params, 0, sizeof(scan_params));
    scan_params.type = 0x00;  // 0x00: Passive scanning
    scan_params.interval = htobs(0x0010);  // 10ms
    scan_params.window = htobs(0x0010);  // 10ms
    // scan_params.own_type = 0x00;  // Public Device Address
    scan_params.filter = 0x00;  // Accept all adv packets
    scan_params_rq.ogf = OGF_LE_CTL;
    scan_params_rq.ocf = OCF_LE_SET_SCAN_PARAMETERS;
    scan_params_rq.cparam = &scan_params;
    scan_params_rq.clen = LE_SET_SCAN_PARAMETERS_CP_SIZE;
    scan_params_rq.rparam = NULL;
    scan_params_rq.rlen = 0;
    if (hci_send_req(hci_socket, &scan_params_rq, 0) < 0) {
        perror("LE Set Scan Parameters command failed");
        exit(1);
    }

    uint8_t enable = 0x01;
    struct hci_request scan_enable_rq;
    le_set_scan_enable_cp scan_enable;
    memset(&scan_enable, 0, sizeof(scan_enable));
    scan_enable.enable = enable;
    scan_enable.filter_dup = 0x01;  // Filter duplicate adv reports
    scan_enable_rq.ogf = OGF_LE_CTL;
    scan_enable_rq.ocf = OCF_LE_SET_SCAN_ENABLE;
    scan_enable_rq.cparam = &scan_enable;
    scan_enable_rq.clen = LE_SET_SCAN_ENABLE_CP_SIZE;
    scan_enable_rq.rparam = NULL;
    scan_enable_rq.rlen = 0;
    if (hci_send_req(hci_socket, &scan_enable_rq, 0) < 0) {
        perror("LE Set Scan Enable command failed");
        exit(1);
    }

    // 设置事件过滤器，仅接收LE Meta事件
    struct hci_filter new_options;
    hci_filter_clear(&new_options);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_options);
    hci_filter_set_event(EVT_LE_META_EVENT, &new_options);
    if (setsockopt(hci_socket, SOL_HCI, HCI_FILTER, &new_options, sizeof(new_options)) < 0) {
        perror("Set socket option failed");
        exit(1);
    }

    printf("Scanning for BLE devices...\n");

    while (1) {
        uint8_t buf[HCI_MAX_EVENT_SIZE];
        ssize_t len = read(hci_socket, buf, sizeof(buf));
        if (len < 0) {
            perror("HCI socket read failed");
            break;
        }

        struct hci_event_hdr *hdr = (struct hci_event_hdr *) buf;
        uint8_t *ptr = buf + (1 + HCI_EVENT_HDR_SIZE);  // Skip event header
							//
	printf("evt:%d\n", hdr->evt);

        if (hdr->evt == EVT_LE_META_EVENT) {
            evt_le_meta_event *meta = (void *) ptr;
            if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
                struct evt_le_advertising_info *info = (void *) (meta->data + 1);
                uint8_t *data = info->data;
                int data_length = info->length;

                // Check if it's an extended advertising PDU
                if (info->evt_type == 0x0D) {  // Extended advertising event type
                    // printf("Discovered device: %s, RSSI=%d dBm\n", bdaddr_to_str(&info->bdaddr), (char)info->data[0]);
                    printf("Discovered , RSSI=%d dBm\n", (char)info->data[0]);
                    printf("Manufacturer Data: ");
                    for (int i = 0; i < data_length; i++) {
                        printf("%02X ", data[i]);
                    }
                    printf("\n");
                    parse_ext_pdu(data, data_length);
                }
            }
        }
    }

    // 关闭LE扫描
    scan_enable.enable = 0x00;
    if (hci_send_req(hci_socket, &scan_enable_rq, 0) < 0) {
        perror("LE Set Scan Disable command failed");
    }

    // 恢复之前的socket选项
    if (setsockopt(hci_socket, SOL_HCI, HCI_FILTER, &old_options, sizeof(old_options)) < 0) {
        perror("Reset socket option failed");
    }

    close(hci_socket);

    return 0;
}

