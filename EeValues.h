/** EeValues.h ** Store record in EE memory **  Brian Witt ** Sept 2013 **/
/*
 *  Allow sotrage of a data record in EE memory.  Class is a header that
 *  is stored along with client's data.  It is expected that all persistent
 *  variables for the Arduino application will be stored together.
 *  However, this class-type can be derived for many data-records if
 *  needed.
 *
 *  As long as there NO virtual methods, the whole object object, including
 *  the EeValues header can be written straight from RAM into EE-memory.
 *
 * #if _EEVALUES_CONF_HUNT_FOR_RECORD
 *   The action stored location in EE memory is hidden from the caller.
 *   The TryRead() method will search for the record using the 'ident' and
 *   ensure a good CRC of the whole record.  If either 'ident' not found
 *   or the CRC is invalid, then the data-record cannot be found.
 * #else
 *   User must know offset of record in EE-memory.  Fields that must match
 *   those already set are: identification (4 bytes), whole size (1 byte),
 *   and CRC (1 byte).
 * #endif
 *
 *    brian witt    Sept 2013    New.
 *    brian witt    Nov 2013     Fields stored in EE memory now own structure.
 */

#ifndef _LIBRARIES_EEVALUES_H
#define _LIBRARIES_EEVALUES_H

#include <inttypes.h>
#include "Arduino.h"            // for 'boolean' and 'byte'


/**  Define 1 to hunt/search for RECORD, 0 for client to explicitly state offset. */
#define _EEVALUES_CONF_HUNT_FOR_RECORD   0


/* ------------------------------------------------------------------- */

// https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#Common-Type-Attributes
#ifdef PACKED
#define  PACKED   __attribute__ ((__packed__))
#endif  /* PACKED */

#define _EEVALUES_VERSION  1

#define _EEVALUES_CRC_SEED  0x81

/** Allow creation of 4 bytes that are 4 characters.  Arduino has 2 byte 'int' types.  LITTLE-ENDIAN version. */
#define MK4CODE(_a,_b,_c,_d)   ( ((uint32_t)(_d) << 24) | ((uint32_t)(_c) << 16) | ((_b) << 8) | (_a) )

typedef uint32_t  EeIdent;

typedef uint16_t  eeoffset_t;
#define  ERR_NO_HEADER  ((eeoffset_t) -1)
#define  ERR_HEADER_BAD_CRC  ((eeoffset_t) -2)


/* ------------------------------------------------------------------- */

class EeValues
{
   public :
#if _EEVALUES_CONF_HUNT_FOR_RECORD
     boolean  findHeader();
#endif

     boolean  isHeaderValid(void);
     int      write(void);

     //  Erase the "user data portion" of the record.  0xFF reduces wear on EE memory.
     void eraseWholeRecord( uint8_t fill_value = 0xff );
     void eraseUserData( uint8_t fill_value = 0xff );

     EeIdent  ident(void) const { return m_header.m_ident; }

     //  User data exists in RAM memory ( not PROGMEM ).
     void     setUserDataPtr( void * user_data ) { m_user_data = user_data; }
     void *   userDataPtr(void) const { return m_user_data; }

     // Pass in size of user record ( that doesn't count EeValues header ).
     void     setUserSize( uint8_t siz ) { m_header.m_full_size = siz + sizeof(EeHeader); }

     //  Portion that is client's, i.e. after the EeHeader stored in front of record.
     //  Client's portion of record, not the actual size in ee-memory.
     unsigned userSize(void) const { return m_header.m_full_size - sizeof(EeHeader); }

     //  Actual size stored, which includes EeValues overhead.
     unsigned realSize() const { return m_header.m_full_size; }

     //  After calling updateCrc8(), here writes from setUserDataPtr() into EE.
     int        writeToEe( void );

     //  Copies from EE into setUserDataPtr() using size in EE header.
     int        writeToUser( void );

     int        writeToUser( eeoffset_t ee_offset, void * user_buffer, size_t ee_count );

     void     updateCrc8();
     uint8_t  crc8() const   { return m_header.m_crc8; }
     void     setCrc8( uint8_t crc ) { m_header.m_crc8 = crc; }

     boolean     setEeOffset( eeoffset_t starting ) { m_start_offset = starting; }

     //  Return EE offset of where header was found.
     eeoffset_t  eeOffset(void) const { return m_start_offset; }

     //  Return EE-memory offset past last read or write.
     eeoffset_t  UUgetLastStoreSffset(void) { return m_start_offset + m_header.m_full_size; }

     EeValues( EeIdent id );

   protected :
     //  Header is stored in EEPROM ahead of user-bytes.
     //  A copy is here for match / finding the 4 char identification.
     //  CRC IS STORED FIRST, WHEN COMPUTING CHECK, CODE ASSUMES THIS.
     struct EeHeader {
        uint8_t        m_crc8;             // valid after call to this->update_crc8()

        uint8_t        m_full_size;        // actual, full size, including EeValues overhead.

        EeIdent        m_ident;
     } m_header;

     eeoffset_t     m_start_offset;

     void *         m_user_data;

#if _EEVALUES_CONF_HUNT_FOR_RECORD
     int       _find_ident();
#endif

   private :
     // no implementation for these:
     EeValues( const EeValues & );
     EeValues& operator=( const EeValues & );
     unsigned char operator==( const EeValues & rhs ) const;
};


#endif
