#include <EEPROM.h>

/* trata de toda parte de configuracao que fica armazenada em EEPROM */

#define CHECKSUM_BASE 3
#define EEPROM_IP 0                                  // 0-IP
#define EEPROM_IP_CS EEPROM_IP+4                     // 4
#define EEPROM_MAC EEPROM_IP_CS+1                    // 5-MAC
#define EEPROM_MAC_CS EEPROM_MAC+6                   // 11
#define EEPROM_NAMES EEPROM_MAC_CS+1                 // 12-10x(NAME+CS)
#define EEPROM_CORS_HEADER EEPROM_NAMES+88           // 93-CORS
#define EEPROM_CORS_HEADER_CS EEPROM_CORS_HEADER+100 // 193

uint8_t EE_checksum(uint8_t index, uint8_t tam){
  uint8_t result = index+CHECKSUM_BASE; // inicia com um valor diferente de 0, para que a soma de zeros não dê zero.
  for (int i=0; i<tam; i++) {
    result+=EEPROM.read(index+i);
  }
  return result;
}

void EEPROM_write(uint8_t address, uint8_t value) {
    if (EEPROM.read(address)!=value) {
      EEPROM.write(address,value);
    } // if
}

boolean EEPROM_read(uint8_t *dest, uint8_t len, uint8_t address) {
  if (EEPROM.read(address+len)==EE_checksum(address,len)) {
    for (int i=0; i<len; i++) {
      dest[i]=EEPROM.read(address+i);
    }
    return true;
  }
  return false;
}

boolean EE_getMAC(uint8_t *mac) {
  return EEPROM_read(mac,6,EEPROM_MAC);
}

boolean EE_getIP(uint8_t *ip) {
  return EEPROM_read(ip,4,EEPROM_IP);
}

boolean EE_getName(uint8_t relayId, char *relayNameDest) {
  uint8_t address = EEPROM_NAMES+(relayId*11); // 11 caracteres por nome. São 10 para o nome e 1 para o checksum.
  return EEPROM_read((uint8_t*)relayNameDest,10,address);
}

void EE_write(uint8_t address, uint8_t* values, int tam) {
  uint8_t checksum=address+CHECKSUM_BASE;
  for(int i=0;i<tam;i++) {
    EEPROM_write(address+i, values[i]);
    checksum+=values[i];
  } // for
  // escreve o checksum
  EEPROM_write(address+tam,checksum);
}

void EE_saveMAC(uint8_t *mac) {
  EE_write(EEPROM_MAC, mac,6);
}

void EE_saveIP(uint8_t *ip) {
  EE_write(EEPROM_IP, ip,4);
}

void EE_saveName(uint8_t relayId, char *relayName) {
  /* relayId must be between 0 and 7 */
  uint8_t address = EEPROM_NAMES+(relayId*11);
  EE_write(address, (uint8_t*)relayName,10);
}
