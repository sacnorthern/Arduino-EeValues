

//  MUST HAVE <crc8.h> ELSE COMPILE FAILS (bwitt, Sept 2013).
#include <crc8.h>
#include <EeValues.h>
#include <Boards.h>        // is libraries\Firmata\Boards.h
#include <CnUtils.h>


/* ------------------------------------------------------------------- */

#define REC_IDENT   MK4CODE('I','D', 'N', 'T')
#define REC_EE_OFFSET   10

struct MyIdent
{
    byte    poll_addr;
    char       SN[ 8 ];
} flim;

EeValues    eeMyIdent(REC_IDENT);

/* ------------------------------------------------------------------- */

// #undef F
// #define F(str) str


void setup() {
  // put your setup code here, to run once:
    Serial.begin(9600);  delay(1000);

    eeMyIdent.setUserDataPtr( & flim );
    eeMyIdent.setUserSize( sizeof(flim) );    

    //  Go look for thing in EE.
  boolean res = true;
#if _EEVALUES_CONF_HUNT_FOR_RECORD
  res = eeMyIdent.findHeader();
  if( res )
  {
      Serial.print( "Found it at EE offset $" );
      Serial.println( eeMyIdent.getLastStoreSffset(), HEX );
  }
#else
  eeMyIdent.setEeOffset( REC_EE_OFFSET );
  res = eeMyIdent.isHeaderValid();
#endif
  if( res )
  {
      Serial.print( F("Found ") );
      Serial.print( eeMyIdent.userSize() );
      Serial.println( F(" USER bytes in EE.") );

      if( eeMyIdent.userSize() == sizeof(flim) )
      {
            eeMyIdent.writeToUser();
      }
      else
      {
          Serial.println( "*** WRONG SIZE ***" );
      }
  }
  else
  {
      Serial.println( F("No record found, creating default one...") );
      flim.poll_addr = 0x41 ;
      memcpy( flim.SN, "12345678", sizeof(flim.SN) );

      eeMyIdent.updateCrc8();
      eeMyIdent.writeToEe();
      
      //  Do a verification cycle, since this is an example sketch!
      eeMyIdent.setEeOffset( REC_EE_OFFSET );
      res = eeMyIdent.isHeaderValid();
      if( res )
      {
          Serial.println( F("Yes, EE block verified!!") );
      }
      else
      {
          Serial.println( F("ERROR: EE BLOCK FAILURE VERIFICATION!!") );
      }
  }

  //  See if can read EE one byte at a time.
  {
      Serial.println();
      struct MyIdent  second_chance;
      memset( & second_chance, 0x55, sizeof(second_chance) );

      eeoffset_t   ee;
      byte *       ptr;

      ee = eeMyIdent.UUgetLastStoreSffset() - 1;

      for( int cnt = eeMyIdent.userSize() ; --cnt >= 0 ; --ee )
      {
          ptr = (byte *) (((byte *)&second_chance) + cnt);
          eeMyIdent.writeToUser( ee, ptr, 1 );
      }
      
      if( memcmp( &flim, &second_chance, sizeof(second_chance) ) == 0 )
      {
          Serial.println( "OK, single byte reading is correct." );
      }
      else
      {
          Serial.println( "ERROR: single byte reading FAILED!!" );
      }
  }

  Serial.println();
  dumpEE( Serial, 8, 32 );
  Serial.println();

  Serial.print( F("Bytes in heap = ") );
  Serial.println( freeRam() );
}

void loop() {
  // put your main code here, to run repeatedly: 
  
}
