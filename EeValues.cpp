/** EeValues.cpp ** Store record in EE memory **  Brian Witt ** Sept 2013 **/
/*
 *  Allow sotrage of a data record in EE-memory.  Class is a header that
 *  is stored along with client's data.  It is expected that all persistent
 *  variables for the Arduino application will be stored together.
 *  However, this class-type can be derived for many data-records if
 *  needed.
 *
 *  =if _EEVALUES_CONF_HUNT_FOR_RECORD
 *    The actual stored location in EE-memory is hidden from the caller.
 *    The TryRead() method will search for the record using the 'ident' and
 *    ensure a good CRC of the whole record.  If either 'ident' not found
 *    or the CRC is invalid, then the data-record cannot be found.
 *  =else
 *    Method TryRead() is told where to look.  If the record changes often,
 *    a wear-leveling scheme should be considered.  A client field could
 *    track the number of updates.  When it overflows, the record is moved.
 *    Client code would look for it at "base + K * move_size" locations.
 *  =endif
 *
 *    brian witt    Sept 2013    New.
 *    brian witt    Nov 2013     Fields stored in EE memory now own structure.
 */

#include <EeValues.h>

#include <CnUtils.h>
#include <crc8.h>

#include <avr/eeprom.h>
// #include <stddef.h>	/* offsetof() */


/*  Define 0/1 to disable/enable debugging messages to 'Serial' */
#define EEVALUES_DEBUG  01

#define EEVALUES_DEBUG_CRC  0

/* ------------------------------------------------------------------- */

//  Static ( class-vars ) that are not stored in EE memory.
int  EeValues::s_start_offset;


//  ASSUME sizeof(EeIdent) == sizeof(uint32_t)
typedef union
{
        uint32_t dword;
        struct
        {
            uint8_t byte0;
            uint8_t byte1;
            uint8_t byte2;
            uint8_t byte3;
        } byte;
} FourBytes;

/* ------------------------------------------------------------------- */

#if EEVALUES_DEBUG_CRC

static uint8_t debug_crc8( uint8_t inCrc, uint8_t data )
{
    uint8_t  updated = ::Crc8( inCrc, data );

    //  Arduino serial-port debug.
    Serial.print( "crc8(" );
    Serial.print( data, HEX );
    Serial.print( ") --> " );
    Serial.println( updated, HEX );

    return( updated );    
}

static uint8_t debug_Crc8Block( uint8_t inCrc, const uint8_t *data, uint8_t len )
{
    while ( len > 0 )
    {
        Serial.print( "blk-" );
        inCrc = debug_crc8( inCrc, *data++ );
        len--;
    }

    return inCrc;
}

#define Crc8(inCrc, data)  debug_crc8((inCrc), (data))
#define Crc8Block(inCrc, ptr, len)   debug_Crc8Block( (inCrc), (ptr), (len) )

#endif

/* ------------------------------------------------------------------- */

#if _EEVALUES_CONF_HUNT_FOR_RECORD
/***
 *   Go looking for 'ident' somewhere in EE-memory.  Failure could be
 *   an invalid CRC as well.
 *   @return true if found, false otherswise.
 *   @seealso E2END last valid EE address, e.g. 1023.
 */
boolean
EeValues::tryRead()
{

    if( _find_ident() >= 0 )
        return true;

    return( false );
}   /* end EeValues::TryRead() */
#endif

/***
 *   Go looking for 'ident' header at offset give in EE-memory.
 *   Failure could be an invalid CRC as well.
 *   Expected size must already be set in record.
 *
 *   @return true if found and update 's_start_offset', false otherswise.
 *   @seealso E2END last valid EE address, e.g. 1023.
 */
boolean
EeValues::tryRead(const uint16_t base_offset)
{
    register uint8_t  match0;
    uint8_t  match1;
    uint8_t  match2;
    uint8_t  match3;
    FourBytes  buff;
    boolean  found = false;

    {
        const FourBytes * p = (const FourBytes *) & this->m_header.m_ident;
        match0 = p->byte.byte0;
        match1 = p->byte.byte1;
        match2 = p->byte.byte2;
        match3 = p->byte.byte3;
    }
#if EEVALUES_DEBUG
    {
        Serial.print("Looking for \"" );

        Serial.print( (char) (this->m_header.m_ident) );
        Serial.print( (char) (this->m_header.m_ident >> 8) );
        Serial.print( (char) (this->m_header.m_ident >> 16) );
        Serial.print( (char) (this->m_header.m_ident >> 24) );

        Serial.print( "\" at offset $" );
        printHexWidth( Serial, base_offset, 2 );
        Serial.println();
    }
#endif

    s_start_offset = base_offset;

    //  Align to where IDENT lives in the record, then check!
//    uint16_t offset = base_offset + offsetof(EeValues, m_ident);
    uint16_t offset = base_offset + offsetof(EeHeader, m_ident);

    buff.dword = eeprom_read_dword( (const uint32_t *) offset );

    if( (buff.byte.byte0 == match0) &&
        (buff.byte.byte1 == match1) &&
        (buff.byte.byte2 == match2) &&
        (buff.byte.byte3 == match3) )
    {
#if EEVALUES_DEBUG
        Serial.println( " .. found IDENT match!" );
#endif
        // Found a matching IDENT. Now inspect size byte.
        if( eeprom_read_byte( (const uint8_t *) (base_offset + offsetof(EeHeader, m_full_size)) ) == m_header.m_full_size )
        {
#if EEVALUES_DEBUG
                Serial.println( " .. found size match!" );
#endif
                // Now check if the EE-CRC is valid by recomputing it and checking for a match....
                match0 = eeprom_read_byte( (const uint8_t *) (base_offset + offsetof(EeHeader, m_crc8) ) );

                //  The EE header looks plausible, so compute CRC of the whole EE image.
                //  Remember to skip EE CRC.  We'll compare it at the end.
                uint16_t  off = base_offset + sizeof(m_header.m_crc8);
                uint8_t   crc = _EEVALUES_CRC_SEED;

                for( uint8_t siz = realSize() - sizeof(m_header.m_crc8) ; siz > 0 ; --siz, ++off )
                {
#if EEVALUES_DEBUG_CRC
                    // DEBUG: prefix crc value with PROGMEM address.
                    printHexWidth( Serial, off, 3 );
                    Serial.print( ": " );
#endif
                    crc = Crc8( crc, eeprom_read_byte( (const uint8_t *) off ) );
                }

#if EEVALUES_DEBUG
                Serial.print( " .. EE crc=0x" );
                Serial.print( match0, HEX );
                Serial.print( " (offset=$" );
#ifdef OFFSET_OF_BRAIN_DAMAGE
        *** gcc version 4.3.2 (WinAVR 20081205) :
        C:\Users\brian\Documents\Arduino\libraries\EeValues\EeValues.cpp: In member function 'boolean EeValues::tryRead(uint16_t)':
        C:\Users\brian\Documents\Arduino\libraries\EeValues\EeValues.cpp:189: warning: invalid access to non-static data member 'EeValues::m_header' of NULL object
        C:\Users\brian\Documents\Arduino\libraries\EeValues\EeValues.cpp:189: warning: (perhaps the 'offsetof' macro was used incorrectly)
                Serial.print( (base_offset + offsetof( EeValues, m_header.m_crc8 ) ) );
#endif
                const unsigned  offs = ((char *) &(((EeValues *)base_offset) -> m_header.m_crc8 )) - (char *) 0;
                printHexWidth( Serial, offs, 3 );

                Serial.print( "), computed CRC=0x" );
                Serial.println( crc, HEX );
#endif
                /* Compare EE read-CRC with EE computed CRC.  Is EE-record valid? */
                if( match0 == crc )
                {
#if EEVALUES_DEBUG
                    Serial.println( " .. found CRC match!" );
#endif
                    //  It is good, so copy user record portion from EE into this object.
                    /*          eeprom_read_block (void *__dst, const void *__src, size_t __n)  */
                    eeprom_read_block( _userDataPtr(), (void *) (base_offset + sizeof(EeHeader)), userSize() );

                    s_start_offset = base_offset;
                    found = true;
                }
        }
    }

    return( found );
}   /* end EeValues::TryRead() */

/***
 *   Just writes the whole object, both EeValues header and the
 *   user's record, into EE-memory.
 *   Make separate call to update CRC, if needed.
 *
 *   Note: EE-memory is slow to write, like 3.3 millliseconds of busy after a
 *         write.  And the CPU is paused for 2 clock cycles after each write
 *        (see Atmel 16u4 manual, Table 5-3 on page 23).
 */
int
EeValues::write()
{

    /*                      eeprom_write_block (const void *_RAM_src, void *_EE_dst, size_t __n)  */
    eeprom_write_block( (const void *) &this->m_header, (void *) s_start_offset, sizeof(this->m_header) );
    eeprom_write_block( (const void *) this->_userDataPtr(), (void *) (s_start_offset + sizeof(this->m_header)), userSize() );

    return( 0 );
}   /* end EeValues::write() */

/* ------------------------------------------------------------------- */

/***
 *  Computes the in-memory CRC and stores that value into the CRC field of the object.
 */
void
EeValues::updateCrc8()
{
    //  Compute CRC for our overhead, then client's portion of the record.
    uint8_t   crc = Crc8Block( _EEVALUES_CRC_SEED,
                                (const uint8_t *) &this->m_header + sizeof(m_header.m_crc8),
                                sizeof(this->m_header) - sizeof(m_header.m_crc8) );
    crc = Crc8Block( crc, _userDataPtr(), userSize() );

    setCrc8( crc );

#if EEVALUES_DEBUG
    Serial.print( "update_crc8() over " );
    Serial.print( realSize() );
    Serial.print( " bytes, CRC=0x" );
    Serial.println( this->m_header.m_crc8, HEX );
#endif

}   /* end EeValues::updateCrc8() */

void
EeValues::eraseUserData( uint8_t fill_value )
{
#if EEVALUES_DEBUG
    Serial.println( "eraseUserData()" );
#endif
    memset( _userDataPtr(), fill_value, userSize() );
}   /* end EeValues::eraseUserData() */

/* ------------------------------------------------------------------- */

#if _EEVALUES_CONF_HUNT_FOR_RECORD
 
int
EeValues::_find_ident()
{
    register uint8_t  match0;
    uint8_t  match1;
    uint8_t  match2;
    uint8_t  match3;
    FourBytes  buff;
    boolean  found = false;

    {
        const FourBytes * p = (const FourBytes *) & this->m_ident;
        match0 = p->byte.byte0;
        match1 = p->byte.byte1;
        match2 = p->byte.byte2;
        match3 = p->byte.byte3;
    }
    
    for( int offset = E2END - realSize() + offsetof(EeHeader, m_ident) ; offset >= 0 ; offset-- )
    {
        if( match0 != eeprom_read_byte( (const uint8_t *) offset ) )
            continue;

        buff.dword = eeprom_read_dword( (const uint32_t *) offset );

        if( (buff.bites[0] == match0) &&
            (buff.bites[1] == match1) &&
            (buff.bites[2] == match2) &&
            (buff.bites[3] == match3) )
        {
            // Found a matching IDENT.  Now check the record size....
            uint8_t  b = eeprom_read_byte( (uint uint8_t *) (offset + offsetof( EeHeader, m_full_size ) ) );
            if( b == this->m_full_size )
            {
                //  record size matches, so do a full-on CRC.
                
                TODO-IMPLEMENT-CRC-CHECK;

                s_start_offset = offset;
                return( offset );
            }
        }
        
    }

    //  Something negative means not found.
    s_start_offset = 0;
    return( -1 );
}
#endif  /* _EEVALUES_CONF_HUNT_FOR_RECORD */
