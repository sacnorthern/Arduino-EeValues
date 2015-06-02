/** EeValues.cpp ** Store record in EE memory **  Brian Witt ** Sept 2013 **/
/*
 *  Allow storage of a data record in EE-memory.  Class is a header that
 *  is stored along with client's data.  It is expected that all persistent
 *  variables for the Arduino application will be stored together.
 *  However, this class-type can be derived for many data-records if
 *  needed.
 *
 *  =if EEVALUES_CONF_HUNT_FOR_RECORD
 *    The actual stored location in EE-memory is hidden from the caller.
 *    The TryRead() method will search for the record using the 'ident' and
 *    ensure a good CRC of the whole record.  If either 'ident' not found
 *    or the CRC is invalid, then the data-record cannot be found.
 *  =else
 *    Method TryRead() is told where to look.  If the record changes often,
 *    a wear-leveling scheme should be considered.  A client field could
 *    track the number of updates.  When it overflows, the record is moved.
 *    Client code would look for it at "base + K * totalSize(void)" locations.
 *  =endif
 *
 *    brian witt    Sept 2013    New.
 *    brian witt    Nov 2013     Fields stored in EE memory now own structure.
 */

/*  Define 0/1 to disable/enable debugging messages to 'Serial' */
#define EEVALUES_DEBUG  01

#define EEVALUES_DEBUG_CRC  0

#include <crc8.h>
#include <avr/eeprom.h>

#include <EeValues.h>

#if EEVALUES_DEBUG_CRC || EEVALUES_DEBUG
//   Ardu 1.5: A library doesn't know about other libraries. https://code.google.com/p/arduino/wiki/BuildProcess
#include "../CnUtils/CnUtils.h"
#endif

/* ------------------------------------------------------------------- */


//  ASSUME sizeof(EeIdent) == sizeof(uint32_t)
typedef union
{
        uint32_t dword;
        struct PACKED
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


EeValues::EeValues( EeIdent id )
{
    // GCC 4.4 can't initialize a field inside an instance-var structure, so do it manually.
    m_header.m_ident = id;
    m_header.m_crc8 = 0;
    m_header.m_full_size = 0;

    m_start_offset = 0;
    m_user_data = NULL;
}


/* static */ unsigned
EeValues::eeSize()
{
    return E2END + 1;    
}


/* ------------------------------------------------------------------- */

/***
 *   Go looking for 'ident' header at offset give in EE-memory.
 *   Size is NOT matched, just 'ident'.
 *   Failure could be an invalid CRC as well.
 *
 *   @return true if found and update 'm_start_offset', false otherwise.
 *   @seealso E2END last valid EE address, e.g. 1023.
 */
boolean
EeValues::isHeaderValid(void)
{
    register uint8_t  match0;
    uint8_t  match1;
    uint8_t  match2;
    uint8_t  match3;
    FourBytes  buff;
    boolean  found = false;

    //  Align to where IDENT lives in the record, then check!
    eeoffset_t  base_offset = eeOffsetOfHeader();
    eeoffset_t  offset      = base_offset + offsetof(EeHeader, m_ident);

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

    buff.dword = eeprom_read_dword( (const uint32_t *) offset );

    if( (buff.byte.byte0 == match0) &&
        (buff.byte.byte1 == match1) &&
        (buff.byte.byte2 == match2) &&
        (buff.byte.byte3 == match3) )
    {
#if EEVALUES_DEBUG
        Serial.println( " .. found Ident-4Code match!" );
#endif
        // Found a matching IDENT.

        // Now check if the EE-CRC is valid by recomputing it and checking for a match....
        match0 = eeprom_read_byte( (const uint8_t *) (base_offset + offsetof(EeHeader, m_crc8) ) );

        //  Remember to skip EE CRC.  We'll compare it afterwards.
        uint16_t  off = base_offset + sizeof(m_header.m_crc8);
        uint8_t   siz = totalSize() - sizeof(m_header.m_crc8);
        uint8_t   crc = _EEVALUES_CRC_SEED;

        for( ; siz > 0 ; --siz, ++off )
        {
#if EEVALUES_DEBUG_CRC
            // DEBUG: prefix crc value with EE address.
            printHexWidth( Serial, off, 3 );
            Serial.print( ": " );
#endif
            crc = Crc8( crc, eeprom_read_byte( (const uint8_t *) off ) );
        }

#if EEVALUES_DEBUG
        Serial.print( " .. EE crc=0x" );
        Serial.print( match0, HEX );
        Serial.print( " (EE offset=$" );

        const unsigned  offs = base_offset + offsetof(EeHeader, m_crc8);
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
            m_header.m_full_size = eeprom_read_byte( (const uint8_t *) (base_offset + offsetof(EeHeader, m_full_size)) );

            m_start_offset = base_offset;
            found = true;
        }
    }

    return( found );
}   /* end EeValues::TryRead() */


int
EeValues::readToUser(void)
{
    /*          eeprom_read_block (void *__dst, const void *__src, size_t __n)  */
    eeprom_read_block( userDataPtr(), (void *) eeOffsetOfUserRecord(), userRecordSize() );

    return( userRecordSize() );
}   // end EeValues::readToUser()


int
EeValues::readToUser( eeoffset_t ee_offset, void * user_buffer, size_t ee_count )
{
#if EEVALUES_DEBUG
    Serial.print( "readToUser( $" );
    Serial.print( ee_offset, HEX );
    Serial.print( ", $" );
    Serial.print( (unsigned) user_buffer, HEX );
    Serial.print( ", " );
    Serial.print( ee_count );
    Serial.println( ")" );
#endif

    eeprom_read_block( user_buffer, (const uint8_t *) ee_offset, ee_count );

    return( ee_count );
}   /* end EeValues::readToUser() */


/***
 *   Just writes the whole object, both EeValues header and the
 *   user's record, into EE-memory.
 *   Make separate call to update CRC, if needed.
 *  Ardu 1.5; routine adds 168 bytes to sketch size (with DEBUG on).
 *
 *   Note: EE-memory is slow to write, like 3.3 milliseconds of busy after a
 *         write.  Also, the CPU is paused for 2 clock cycles after each write
 *        (see Atmel 16u4 manual, Table 5-3 on page 23).
 */
int
EeValues::writeToEe(void)
{

    /*                      eeprom_write_block (const void *_RAM_src, void *_EE_dst, size_t __n)  */
    eeprom_write_block( (const void *) &this->m_header, (void *) m_start_offset, sizeof(this->m_header) );
    eeprom_write_block( (const void *) this->userDataPtr(), (void *) eeOffsetOfUserRecord(), userRecordSize() );

    return( totalSize() );
}   /* end EeValues::writeToEe() */

/* ------------------------------------------------------------------- */

/***
 *  Computes the in-memory CRC and stores that value into the CRC field of the object.
 *  Ardu 1.5; routine adds 170 bytes to sketch size (with DEBUG on).
 */
void
EeValues::updateCrc8()
{
    //  Compute CRC for our overhead, then client's portion of the record.
    uint8_t   crc = Crc8Block( _EEVALUES_CRC_SEED,
                                (const uint8_t *) &this->m_header + sizeof(m_header.m_crc8),
                                sizeof(this->m_header) - sizeof(m_header.m_crc8) );

    crc = Crc8Block( crc, (const uint8_t *) userDataPtr(), userRecordSize() );

    setCrc8( crc );

#if EEVALUES_DEBUG
    Serial.print( "updateCrc8() over " );
    Serial.print( totalSize() );
    Serial.print( " bytes, CRC=0x" );
    printHexWidth( Serial, this->m_header.m_crc8, 2 );
    Serial.println();
#endif

}   /* end EeValues::updateCrc8() */


//  Erase the EeValues header in EE memory.
void
EeValues::eraseEeHeader()
{
#if EEVALUES_DEBUG
    Serial.println( "eraseEeHeader()" );
#endif
    const uint8_t  fill_value = 0xFF;
    for( int off = sizeof(this->m_header) ; --off >= 0 ; )
    {
        eeprom_write_byte( (uint8_t *) (off + m_start_offset), fill_value );
    }
}


void
EeValues::eraseEeUserData( uint8_t fill_value )
{
#if EEVALUES_DEBUG
    Serial.println( "eraseEeUserData()" );
#endif
    for( int off = userRecordSize() ; --off >= 0 ; )
    {
        eeprom_write_byte( (uint8_t *) (off + m_start_offset), fill_value );
    }
}   /* end EeValues::eraseUserData() */


 //  Erase our header and "user data portion".
void
EeValues::eraseWholeRecord( uint8_t fill_value )
{
    eraseEeHeader();
    eraseEeUserData( fill_value );
}
 
 
/* ------------------------------------------------------------------- */

#if EEVALUES_CONF_HUNT_FOR_RECORD

/***
 *   Go looking for 'ident' somewhere in EE-memory.  Failure could be
 *   an invalid CRC as well.
 *   @return true if found, false otherwise.
 *   @seealso E2END last valid EE address, e.g. 1023.
 */
boolean
EeValues::findHeader()
{

    if( _find_ident() >= 0 )
        return true;

    return( false );
}   /* end EeValues::TryRead() */

int
EeValues::_find_ident()
{
    register uint8_t  match0;
    uint8_t  match1;
    uint8_t  match2;
    uint8_t  match3;
    FourBytes  buff;

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

        Serial.println("\"");
    }
#endif

    const int  last_ee = E2END - sizeof(EeHeader) + offsetof(EeHeader, m_ident);

    //  Loop goes looking for a matching IDENT.  However, check logic is based from start
    //  of EeHeader structure.
    for( uint16_t ident_offset = eeOffsetOfHeader() + offsetof(EeHeader, m_ident) ; ident_offset <= last_ee ; ident_offset++ )
    {
        if( match0 != eeprom_read_byte( (const uint8_t *) ident_offset ) )
            continue;

        buff.dword = eeprom_read_dword( (const uint32_t *) ident_offset );

        if( (buff.byte.byte0 == match0) &&
            (buff.byte.byte1 == match1) &&
            (buff.byte.byte2 == match2) &&
            (buff.byte.byte3 == match3) )
        {
            const uint16_t  base_offset = (uint16_t) (ident_offset - offsetof(EeHeader, m_ident));

#if EEVALUES_DEBUG
            Serial.print( " .. found Ident-4Code match at EE $" );
            Serial.println( base_offset, HEX );
#endif
            // Found a matching IDENT.

            // Now check if the EE-CRC is valid by recomputing it and checking for a match....
            match0 = eeprom_read_byte( (const uint8_t *) (base_offset + offsetof(EeHeader, m_crc8) ) );

            //  Remember to skip EE CRC.  We'll compare it afterwards.
            uint16_t  off = base_offset + sizeof(m_header.m_crc8);
            uint8_t   siz = totalSize() - sizeof(m_header.m_crc8);
            uint8_t   crc = _EEVALUES_CRC_SEED;

            for( ; siz > 0 ; --siz, ++off )
            {
#if EEVALUES_DEBUG_CRC
                // DEBUG: prefix crc value with EE address.
                printHexWidth( Serial, off, 3 );
                Serial.print( ": " );
#endif
                crc = Crc8( crc, eeprom_read_byte( (const uint8_t *) off ) );
            }

#if EEVALUES_DEBUG
            Serial.print( " .. EE crc=0x" );
            Serial.print( match0, HEX );
            Serial.print( " (EE offset=$" );

            const unsigned  offs = base_offset + offsetof(EeHeader, m_crc8);
            printHexWidth( Serial, offs, 3 );

            Serial.print( "), computed CRC=0x" );
            Serial.print( crc, HEX );
#endif
            /* Compare EE read-CRC with EE computed CRC.  Is EE-record valid? */
            if( match0 == crc )
            {
#if EEVALUES_DEBUG
                Serial.print( " .. found CRC match at $" );
                printHexWidth( Serial, base_offset, 3 );
                Serial.println();
#endif
                m_header.m_full_size = eeprom_read_byte( (const uint8_t *) (base_offset + offsetof(EeHeader, m_full_size)) );

                m_start_offset = base_offset;
                return ( base_offset );
            }
#if EEVALUES_DEBUG
            else
            {
                Serial.println( " .. no match." );
            }
#endif
        }   // if IDENT matches..

    }

    //  Something negative means not found.
    m_start_offset = 0;
    return( -1 );
}

#endif  /* EEVALUES_CONF_HUNT_FOR_RECORD */
