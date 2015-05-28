
//  MUST HAVE <crc8.h> ELSE COMPILE FAILS (bwitt, Sept 2013).
#include <crc8.h>
#include <EeValues.h>
#include <Boards.h>        // is libraries\Firmata\Boards.h
#include <Firmata.h>
#include <Servo.h>

uint32_t  gaurd_top = 0x11223355L;
extern uint32_t  gaurd_bottom;

extern char *__brkval;        // from "malloc.c"

/* ------------------------------------------------------------------- */

#define REC_IDENT   MK4CODE('C','M', 'R', 'I')
#define REC_EE_OFFSET   10


struct MyRecord : EeValues
{
    uint16_t  baud_div100;
    uint8_t   unit_addr;

    uint8_t   pin_config[ TOTAL_PINS ];        // 0xFF = unused.
    
  MyRecord() : EeValues( REC_IDENT, sizeof(MyRecord) ) {}
  
} flim;

/* ------------------------------------------------------------------- */

#define SERVOS_STATIC  01

extern uint8_t ServoCount;
extern int   servo_last;

#define SERVOS_MAX    6

#if SERVOS_STATIC
// Pins 8-12 are best for servos.
Servo   servos[ SERVOS_MAX ];
#define SERVO_PTR(_p)  ( & servos[_p] )
#else
// Pins 8-12 are best for servos.
Servo *   servos[ SERVOS_MAX ];
#define SERVO_PTR(_p)  ( servos[_p] )
#endif

uint8_t  servo_index[ SERVOS_MAX ];



/* ------------------------------------------------------------------- */

void setup() {

  Serial.begin(9600);
  delay(2000);
  Serial.println( servo_last );
  servo_last = 1;

  Serial.print( "Servo::MAX_SERVOS = " );
  Serial.println( MAX_SERVOS );

  boolean res = true;
#if _EEVALUES_CONF_HUNT_FOR_RECORD
  res = flim.TryRead();
#else
  res = flim.TryRead( REC_EE_OFFSET );
#endif
  if( !res )
  {
    Serial.println( "not found...." );
    Serial.print( "total pins = " );
    Serial.println( TOTAL_PINS, DEC );

    flim.baud_div100 = 9600 / 100;
    flim.unit_addr = 1;

    memset( &flim.pin_config[0], 0xFF, sizeof(flim.pin_config) );    // disable all.
    
    flim.pin_config[ VERSION_BLINK_PIN ] = OUTPUT;    // the LED !!
    flim.pin_config[ 9 ] = SERVO;

    Serial.print( "sizeof(EeValues) = " );
    Serial.println( sizeof(EeValues), DEC );
    flim.Write();
  }
  
  EeValues::dump_ee( Serial, REC_EE_OFFSET - 2, 30 );

  Serial.print( "sizeof(int) = " );
  Serial.print( sizeof(int) );
  Serial.print( ", RAMEND = $" );
  Serial.println( RAMEND, HEX );

  //  Configure pins, either Digi-in, Digi-out, servo, PWM.
// pin modes
//#define INPUT                 0x00 // defined in wiring.h
//#define OUTPUT                0x01 // defined in wiring.h
//#define ANALOG                  0x02 // analog pin in analogInput mode
//#define PWM                     0x03 // digital pin in PWM output mode
//#define SERVO                   0x04 // digital pin in Servo output mode
//#define SHIFT                   0x05 // shiftIn/shiftOut mode
//#define I2C                     0x06 // pin included in I2C setup

  uint8_t  next_servo = 0;

  // Skip pins 0 and 1 (RX and TX).
  for( uint8_t pin = TOTAL_PINS ; --pin >= 2 ; )
  {
      switch( flim.pin_config[pin] )
      {
          case INPUT :
              pinMode( pin, INPUT) ; 
              // digitalWrite( pin, LOW );    // disable internal pull up.
              break;
              
          case OUTPUT :
              pinMode( pin, OUTPUT) ; 
              // digitalWrite( pin, HIGH );
              break;
              
          case SERVO :
              if( next_servo < SERVOS_MAX )
              {
                  Serial.print( "Pin " );
                  Serial.print( pin );
                  Serial.println( " is servo." );

#if SERVOS_STATIC
#else
                  servos[ next_servo ] = new Servo();                  
#endif
                  int res = SERVO_PTR( next_servo )->attach( pin );
                  Serial.print( "result = " );
                  Serial.print(  res );
                  Serial.println( SERVO_PTR( next_servo )->attached() ? " attached" : " NOT attached" );
                  Serial.print( "  Servo::ServoCount = " );
                  Serial.println( ServoCount );
                  
                  servo_index [ next_servo ] = pin;
                  ++next_servo;
              }
              else
              {
                  Serial.print( "ERROR: Pin " );
                  Serial.print( pin );
                  Serial.println( " cannot be serial, no more channels available." );
              }
              break;
              
          default :
              //* Serial.print( "Pin " );
              //* Serial.print( pin );
              //* Serial.println( " not assigned." );
              break;
      }
  }

  //  The rest are not used.
  while( next_servo < SERVOS_MAX )
      servo_index[ next_servo++ ] = INVALID_SERVO;
  Serial.print( "__brkval = $" );
  Serial.println( (__brkval - (char *)0), HEX );

  //  wait for transmission of outgoing data to finish.
  Serial.end();
  delay(1+1);    // 1 ms sleep for UART hardware buffer size, 1 byte in shift reg.

  Serial.begin( (unsigned long)flim.baud_div100 * 100 );
  delay(100);

  Serial.println();
  Serial.println( "Program now runs..." );
}

void loop() {
  // put your main code here, to run repeatedly: 
  //  Serial.read() is non-blocking; it returns -1 if no data.
  int  ch = Serial.read();
  if( -1 == ch || 0x20 > ch )
      return ;
      
  
  if( '0' <= ch && ch <= '9' )
  {
      //  Allow servo to be adjusted.
      int  angle = map( ch, '0', '9', 0+9, 180-9 );
      SERVO_PTR( 0 )->write( angle );
      Serial.print( "Angle " );
      Serial.println( angle );
  } else
  if( ch == '#' )
  {
      //  "##" to erase EE-memory.
      while( (ch = Serial.read()) == -1 );
      if( ch == '#' )
      {
          eeprom_write_word( (uint16_t *) flim.get_last_store_offset(), 0xbeef );
          Serial.println( "EE-memory zapped!" );
      }
  }
  else
  {
      //  command char not recognized..
      Serial.print( '?' );
      Serial.print( gaurd_top, HEX );
      Serial.print( '-' );
      Serial.println( gaurd_bottom, HEX );
  }

}  // end loop()

uint32_t  gaurd_bottom = 0xAABBDDEEL;

