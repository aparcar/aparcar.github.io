/*
 * Describes the currently running version. This value is transmitted next to
 * the battery voltage and helps to keep track what software version is running
 * on nodes. It should be increased whenever a significant change to logic
 * happens which may change the behaviour of a running device.
 */
#define VERSION 4

// How often to send battery voltage (and version) in minutes
#define BATTERY_SEND_INTERVAL 360 // 6 hours
// How often to send OneWire temperature in minutes
#define ONEWIRE_SEND_INTERVAL 30
// How often to send MB7389 sonic distance in minutes
#define SONIC_SEND_INTERVAL 10
// How often to send VH400 moisture in minutes
#define VH400_SEND_INTERVAL 3
// How often to send Davis wind speed in minutes
#define DAVIS_SPEED_SEND_INTERVAL 2
// How often to send Davis wind direction in minutes
#define DAVIS_DIRECTION_SEND_INTERVAL 2

/*
 * Setting PINs to devices will enable them within the code. The default values
 * presented here work fine with Heltec CubeCell boards, allowing to attach all
 * sensor at once. However it is also possible to remove any PIN definition and
 * thereby disable the sensor completely.
 */
// #define ONEWIRE_PIN GPIO4
//#define SONIC_RX_PIN GPIO1
//#define RAIN_GAUGE_PIN GPIO5
//#define VH400_PIN ADC2
#define DAVIS_SPEED_PIN GPIO5
#define DAVIS_DIRECTION_PIN ADC2

// This defines the fallback value of mm/t if not provided by the node
// configuration file. The default value corresponds to a HOBO RG3.
#define DEFAULT_MM_PER_COUNT 0.254 // 0.01"

/*
 * All options below are advanced and specifically for the LoRaWAN library used.
 * None of these values should require manual changes except when using outside
 * the US915 zone or using ABP rather than OTAA.
 */

// OTAA parameters should be set via AT commands in the configuration file.
uint8_t devEui[] = {0xC0, 0xFF, 0xEE, 0xC0, 0xFF, 0xEE, 0xCA, 0xFE};
uint8_t appEui[] = {0xC0, 0xFF, 0xEE, 0xC0, 0xFF, 0xEE, 0xCA, 0xFE};
uint8_t appKey[] = {0xC0, 0xFF, 0xEE, 0xC0, 0xFF, 0xEE, 0xC0, 0xFF,
                    0xEE, 0xC0, 0xFF, 0xEE, 0xC0, 0xFF, 0xEE, 0x42};

/*
 * While it is possible to use ABP it is not recommended, please use OTAA!
 * OTAA is more secure and allows the node and backend to negotiate an ideal
 * transmission rate to safe power and air-time.
 */
uint8_t nwkSKey[] = {};
uint8_t appSKey[] = {};
uint32_t devAddr = (uint32_t)0x0;

/*LoraWan channelsmask, default channels 0-7 sub 2*/
uint16_t userChannelsMask[6] = {0xFF00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t loraWanClass = LORAWAN_CLASS;

/*OTAA or ABP*/
bool overTheAirActivation = LORAWAN_NETMODE;

/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;

/* set LORAWAN_Net_Reserve ON, the node could save the network info to flash,
 * when node reset not need to join again */
// bool keepNet = LORAWAN_NET_RESERVE;
bool keepNet = false;

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = false;

/*!
 * Number of trials to transmit the frame, if the LoRaMAC layer did not
 * receive an acknowledgment. The MAC performs a datarate adaptation,
 * according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
 * to the following table:
 *
 * Transmission nb | Data Rate
 * ----------------|-----------
 * 1 (first)       | DR
 * 2               | DR
 * 3               | max(DR-1,0)
 * 4               | max(DR-1,0)
 * 5               | max(DR-2,0)
 * 6               | max(DR-2,0)
 * 7               | max(DR-3,0)
 * 8               | max(DR-3,0)
 *
 * Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
 * the datarate, in case the LoRaMAC layer did not receive an acknowledgment
 */
uint8_t confirmedNbTrials = 4;
