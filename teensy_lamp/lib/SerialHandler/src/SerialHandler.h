/*"""

 Serial Message Format:
 First character of the message is the message type for both sending and reception.
 The message type is followed by the message payload.

   - While sending to haptic device:
        "G" + <Gripper Width> + SERIAL_SEPERATOR_CH + <Gripper Speed> + SERIAL_SEPERATOR_CH + <Gripper Force>
        "C" + <Working Modes or Commands>

    - While receiving from haptic device:
        "T" + <Next Point On The Trajectory>

"""*/
#ifndef SerialHandler_H
#define SerialHandler_H
#include "Arduino.h"
#include "advancedSerial.h"
#include <inttypes.h>

enum SerialConnectionApp {
    UNITY = 0,
    HAPTIC_SERIAL_INTERFACE = 1,
};

class SerialHandler : public advancedSerial {
public:
    void update();
    void setSerial(Stream &serial);
    void setStartMarker(char startMarker);
    void setEndMarker(char endMarker);
    void setSeperator(char seperator);
    void setDebug(bool debug);
    void setPrintFrequency(float printFrequency);
    void parseString(char *string);
    char getStartMarker();
    char getEndMarker();
    Stream &getSerial();

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t mode = 0;

private:
    float _printFrequency = 1;
    bool _debug = false;
    bool _serialOpen = false;
    bool _serialOpenPrev = false;
    long _periodicTimer = 0;
    Stream *_serial;
    char _startMarker = '<';
    char _endMarker = '>';
    char _separator = '#';
    static const byte _numChars = 128;
    char _receivedChars[_numChars];
    void _printPeriodically(float frequency, bool debug);
    void _receiveNonBlocking(void);
};

#include "Arduino.h"
#endif