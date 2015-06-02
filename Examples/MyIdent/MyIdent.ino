/***  libraries/EeValues/Examples/MyIdent/MyIdent.ino  June 2015, brian witt ***/

//  Ardu 1.5: Required to make EeValues class compile OK.
#include <crc8.h>

#include <EeValues.h>
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

void dump_some_ee()
{
  Serial.println();
  dumpEE( Serial, 0, 64 );
  Serial.println();
}   /* end dump_some_ee() */


boolean validate_record()
{
    Serial.println();

    boolean  res = false;

    struct MyIdent  second_chance;
    memset( & second_chance, 0x55, sizeof(second_chance) );

      eeoffset_t   ee;
      byte *       ptr;

      ee = eeMyIdent.lastStoredOffset() - 1;

      for( int cnt = eeMyIdent.userRecordSize() ; --cnt >= 0 ; --ee )
      {
          ptr = (byte *) (((byte *)&second_chance) + cnt);
          eeMyIdent.readToUser( ee, ptr, 1 );
      }
      
      if( memcmp( &flim, &second_chance, sizeof(second_chance) ) == 0 )
      {
          Serial.println( "OK, single byte reading is correct." );
          res = true;
      }
      else
      {
          Serial.println( "ERROR: single byte reading FAILED!!" );
      }

    return( res );
}   /* end validate_record() */


void
update_poll_addr()
{

      if( eeMyIdent.userRecordSize() == sizeof(flim) )
      {
            eeMyIdent.readToUser();

            flim.poll_addr += 1;
            
            eeMyIdent.updateCrc8();
            eeMyIdent.writeToEe();
      }
      else
      {
          Serial.println( "*** WRONG SIZE ***" );
      }

}   /* end update_poll_addr() */


/* ------------------------------------------------------------------- */


void setup() {
  // put your setup code here, to run once:
    Serial.begin(9600);  delay(1000);

    eeMyIdent.setUserDataPtr( & flim );
    eeMyIdent.setUserSize( sizeof(flim) );    

    //  Go look for thing in EE.
  boolean res = false;

#if EEVALUES_CONF_HUNT_FOR_RECORD
  res = eeMyIdent.findHeader();
  if( res )
  {
      Serial.print( "Found it at EE offset $" );
      printHexWidth( Serial, eeMyIdent.eeOffsetOfHeader(), 3 );
      Serial.println();
  }
  else
  {
      //  If none found, provide a default location in EEMEM.
      Serial.println( "None found, so providing default location" );
      eeMyIdent.setEeOffset( REC_EE_OFFSET );
  }
#else
  eeMyIdent.setEeOffset( REC_EE_OFFSET );
  res = eeMyIdent.isHeaderValid();
#endif
  if( res )
  {
      Serial.print( F("Found ") );
      Serial.print( eeMyIdent.userRecordSize() );
      Serial.println( F(" USER bytes in EE.") );

      if( eeMyIdent.userRecordSize() == sizeof(flim) )
      {
            eeMyIdent.readToUser();
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
  validate_record();

  dump_some_ee();

  Serial.print( F("Bytes in heap = ") );
  Serial.println( freeRam() );
  
  //  Menu of commands.
  Serial.println( "Commands:  E = Erase EEMEM ; D = Dump some EEMEM ; V = Validate Record ;" );
  Serial.println( "           U = Update record." );
  Serial.println();
}

int  ch;

void loop() {

    // send data only when you receive data:
    if (Serial.available() > 0 && (ch = Serial.read()) > 0 )
    {
        if( 'a' <= ch && ch <= 'z' )        // TOUPPER(ch)
            ch -= 0x020;

        // read the incoming byte.
        switch( ch )
        {
            case 'E' :
                {
                EeValues  erasure( 0 );
                erasure.setEeOffset( 0 );
                erasure.setUserSize( erasure.eeSize() - 16 );
                erasure.eraseWholeRecord();
                }
     
                 Serial.println( "EE erased." );
                 break;
                 
             case 'D' :
                 dump_some_ee();
                 break;

            case 'U' :
                update_poll_addr();
                break;

            case 'V' :
                validate_record();
                break;
        }
    }
}
