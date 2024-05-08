# LYJ: if can not run try : sudo hciconfig hci0 down && sudo hciconfig hci0 up
from bluepy.btle import Scanner, DefaultDelegate

class ScanDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    def handleDiscovery(self, dev, isNewDev, isNewData):
        #if isNewDev and dev.addrType == 0:
        # print(f"Discovered device: {dev.addr} ({dev.addrType}), RSSI={dev.rssi} dBm")
        for (adtype, desc, value) in dev.getScanData():
            # print(f"desc:{desc}");
            if desc == "Complete Local Name":
                print("Name:", value)
            if desc == "Complete 128b Services":
                print("UUID128:", value)
                print(f"Discovered device: {dev.addr} ({dev.addrType}), RSSI={dev.rssi} dBm, connectable={dev.connectable}")

def main():
    scanner = Scanner().withDelegate(ScanDelegate())
    print("Scanning for BLE devices...")
    scanner.scan(10.0)

if __name__ == "__main__":
    main()
