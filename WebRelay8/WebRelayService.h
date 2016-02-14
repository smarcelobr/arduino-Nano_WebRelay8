#ifndef WEB_RELAY_SERVICE_H
#define WEB_RELAY_SERVICE_H
// Number of relays on board, max 9 are supported!
#define NUM_RELAYS 8

// Service error code
#define NO_ERROR 0
/* ERROR_INVALID_RELAY_ID - The application only supports relay numbers between 0 and the value in 
 * 		 numRelays. Change it if you have more relays.
 */
#define ERROR_INVALID_RELAY_ID 201
/* ERROR_INVALID_RELAY_STATUS - The application only supports the values 0 for off, 1 for on and 2 to 
 * 		 invert the status, everything else will not work!
 */
#define ERROR_INVALID_RELAY_STATUS 202
/* ERROR_INVALID_RELAY_NAME_FORMAT - Nome do relay está com caracteres ou tamanho invalido */
#define ERROR_INVALID_RELAY_NAME_FORMAT 203
/* ERROR_INVALID_CHECKSUM_EEPROM - O checksum da eprom não confere com a informação cadastradas */
#define ERROR_INVALID_CHECKSUM_EEPROM 204

// supported relay status
#define R_OFF 0
#define R_ON  1
#define R_INV 2

// Structures for requests:
typedef struct  {
  uint8_t numRelay;
  uint8_t newOnOffStat;
} ChangeStatusRequest;

typedef struct {
  uint8_t numRelay;
  char* newName;
} ChangeNameRequest;

// class:
class RelayService {
private:
  // Output ports for relays, change it if you connected other pins, must be adjusted if number of relays changed
  int *outputPorts_P;
  // this is for performance, must be adjusted if number of relays changed
  boolean portStatus[NUM_RELAYS];

public:
  RelayService(int pins_P[NUM_RELAYS]);
  boolean getStatus(uint8_t numRelay) const;
  void getName(uint8_t numRelay,char dest[10]) const;
  uint8_t changeStatus(ChangeStatusRequest &chgStatReq);
  uint8_t changeName(ChangeNameRequest &chgNameReq);
};

#endif // WEB_RELAY_SERVICE_H
