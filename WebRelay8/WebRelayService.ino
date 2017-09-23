
RelayService::RelayService(const int pins_P[NUM_RELAYS]):outputPorts_P(pins_P) {
	// all pins will be turned off
	for (int i = 0; i < NUM_RELAYS; i++) {
                int pin = pgm_read_byte(&outputPorts_P[i]);
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
                portStatus[i]=false;
	}
}

boolean RelayService::getStatus(uint8_t numRelay) const {
  return portStatus[numRelay];
}

void RelayService::getName(uint8_t numRelay,char dest[10]) const {
	dest[0]=0x00;
	if (numRelay>=NUM_RELAYS) {
		return; // numero de relay invalido
	}
	if (!EE_getName(numRelay,dest)) {
		strcpy_P(dest, PSTR("RELAY "));
		dest[6]=('0'+numRelay);
		dest[7]=0x00; // end of string
	}
}

/**
 * Performs the change on the relay itself.
 */
uint8_t RelayService::changeStatus(ChangeStatusRequest &chgStatReq) {
	#ifdef DEBUGGING
		Serial.print(F("The target status of the relay ")); Serial.print(chgStatReq.numRelay);
		Serial.print(F(" is: ")); Serial.println(chgStatReq.newOnOffStat);
	#endif
	if (chgStatReq.numRelay>=NUM_RELAYS) {
		return ERROR_INVALID_RELAY_ID; // numero de relay nao existe
	}
	int pin = pgm_read_byte(&outputPorts_P[chgStatReq.numRelay]);
	bool target;
	switch (chgStatReq.newOnOffStat) {
	case R_OFF:
		target=false;
		break;
	case R_ON:
		target=true;
		break;
	case R_INV:
		target = !portStatus[chgStatReq.numRelay];
		break;
	default:
		return ERROR_INVALID_RELAY_STATUS;
	}
    	#ifdef DEBUGGING
    		Serial.print(F("Curr status is:")); Serial.println(portStatus[chgStatReq.numRelay]);
    		Serial.print(F("Target status is:")); Serial.println(target);
    	#endif
	if(portStatus[chgStatReq.numRelay] != target) {
		digitalWrite(pin, target ? HIGH : LOW);
		portStatus[chgStatReq.numRelay] = target;
	}
	return NO_ERROR;
}

/*
  Altera o nome de um rele. Retorna 0 se tudo ok, ou um codigo de 
  erro caso ocorra um problema.
*/
uint8_t RelayService::changeName(ChangeNameRequest &chgNameReq) {
	// valida a requisicao
	if (chgNameReq.numRelay>=NUM_RELAYS) {
		return ERROR_INVALID_RELAY_ID; // numero de relay nao existe
	}
	// o nome deve ter no minimo 1 e no maximo 10 caracteres
	int len = strlen(chgNameReq.newName);
	if (len<1 || len>10) {
		return ERROR_INVALID_RELAY_NAME_FORMAT;
	}
	// testa se os caracteres são validos:
	for (int i=0;i<strlen(chgNameReq.newName);i++) {
		int ch=chgNameReq.newName[i];
		if (ch<32 || ch>126) return ERROR_INVALID_RELAY_NAME_FORMAT;      
	}
	// Como o nome e a chave são validas, altera na EEPROM:
	EE_saveName(chgNameReq.numRelay,chgNameReq.newName);
	return 0;
}

