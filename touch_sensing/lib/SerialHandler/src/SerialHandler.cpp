#include "SerialHandler.h"

/*"""

 Serial Message Format:
 Every message sent and received over the serial should start with a '_startMarker' and end with an '_endMarker'.
 First character (after the start character) of the message is the message type for both sending and reception.
 The message type is followed by the message payload.
 Different values inside the payload are separated by a '_separator'.
 SERIAL_SEPERATOR_CH = '#'
_separator = '#'
"""*/
void SerialHandler::update() {
    _printPeriodically(_printFrequency, _debug);
    _receiveNonBlocking();
}

void SerialHandler::setSerial(Stream &serial) {
    _serial = &serial;
    advancedSerial::setPrinter(serial);
}

void SerialHandler::setModeVar(int &mode) { this->mode = &mode; }

void SerialHandler::setEndMarker(char endMarker) { _endMarker = endMarker; }

void SerialHandler::setStartMarker(char startMarker) { _startMarker = startMarker; }

void SerialHandler::setSeperator(char seperator) { _separator = seperator; }

void SerialHandler::_receiveNonBlocking() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char rc;

    if (_serial->available() <= 0)
        return;

    rc = _serial->read();
    if (recvInProgress == true) {
        if (rc != _endMarker) {
            _receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= _numChars) {
                this->pln("Buffer Overflow");
                ndx = _numChars - 1;
            }
        } else {
            // digitalWrite(13, !digitalRead(13));
            _receivedChars[ndx] = '\0'; // terminate the string when the end marker arrives
            recvInProgress = false;
            ndx = 0;
            this->parseString(_receivedChars);
        }
    } else if (rc == _startMarker) {
        recvInProgress = true;
    } else {
        Serial.print(rc);
    }
}

void SerialHandler::parseString(char *string) {
    const char seperator[2] = {_separator, '\0'};
    char messageType = string[0];
    // Remove the first character from the string using memmove. First character is the message type.
    memmove(string, string + 1, strlen(string));

    switch (messageType) {
    case 'C':{

        // Parse the message in the following format: <Cff365d>
        // Start, end and messagetype characters are already removed. So it is ff365d which is a hex color code.
        // Convert the hex color code to CRGB and set the wholeStripColor to that color. (Do not use strtoul)
        // who
        long hexColor = strtol(string, NULL, 16);
        byte r = (hexColor >> 16) & 0xFF;
        byte g = (hexColor >> 8) & 0xFF;
        byte b = hexColor & 0xFF;
        wholeStripColor = CRGB(r, g, b);
        Serial.print(string);
        Serial.print(" ");
        Serial.print(r);
        Serial.print(" ");
        Serial.print(g);
        Serial.print(" ");
        Serial.println(b);

        break;
    }
    case 'M': {

        // Parse the message in the following format: <M0>
        // Start, end and messagetype characters are already removed. So it is 0 which is a number.
        // Set the mode to that number.
        // who
        int val = atoi(string);
        *mode = val;
        Serial.print(string);
        Serial.print(" ");
        Serial.println(val);
        break;
    }
    }
    return;
}

void SerialHandler::_printPeriodically(float freq, bool debug = false) {
    if (freq <= 0)
        return;
    // Guard close to when the next print should happen
    if (millis() - _periodicTimer < 1000.0 / freq)
        return;
    _periodicTimer = millis();
}

void SerialHandler::setPrintFrequency(float freq) { _printFrequency = freq; }
void SerialHandler::setDebug(bool debug) { _debug = debug; }

Stream &SerialHandler::getSerial() {
    Stream *serial = &*_serial;
    return *serial;
}

char SerialHandler::getStartMarker() { return _startMarker; }
char SerialHandler::getEndMarker() { return _endMarker; }