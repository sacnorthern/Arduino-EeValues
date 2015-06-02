/** EeValues.h ** Store record in EE memory **  Brian Witt ** Sept 2013 **/
/*
 *  Allow storage of a data record in EE memory.  A header provides a CRC
 *  check-value, an identification, and size of user-data.  Application can
 *  update its data record, update CRC, and then write to EEMEM.  It
 *  can read back the whole user record ( if buffer space is available ),
 *  or read in chunks.  The EEMEM record can also be erased.  On 8-bit
 *  ATMEL's, EEMEM is not large, like maybe 256 or 2048 bytes.
 * 
 *  The object provides a pointer into EEMEM.  The object holds a 4-byte
 *  identification value, e.g. 4 ASCII chars, along with the CRC over the
 *  header and user-data.  The very important item held by the object is
 *  the starting offset in EEMEM ; however, the application usually has
 *  this hard-coded.  Therefore, this object can be allocated on the
 *  stack during setup and "write settings to EEMEM" operation.  Yet,
 *  the object is small enough to keep around....
 *
 *  Reading and Writing ... This object represents EEMEM, so reading and
 *  writing are relative to EEMEM.  So to copy from EEMEM into a user RAM
 *  structure is "reading".  Putting bytes into EEMEM is "writing".
 *
 *
 * #if EEVALUES_CONF_HUNT_FOR_RECORD
 *   The action stored location in EE memory is hidden from the caller.
 *   The TryRead() method will search for the record using the 'ident' and
 *   ensure a good CRC of the whole record.  If either 'ident' not found
 *   or the CRC is invalid, then the data-record cannot be found.
 * #else
 *   User must know offset of record in EE-memory.  The identification
 *   field must match, and the CRC must be valid.  It is up to the
 *   application to determine if the size is reasonable or not.
 * #endif
 *
 *    brian witt    Sept 2013    New.
 *    brian witt    Nov 2013     Fields stored in EE memory now own structure.
 *    brian witt    June 2015    Revise and clean-up.
 */

#ifndef _LIBRARIES_EEVALUES_H
#define _LIBRARIES_EEVALUES_H

#include <inttypes.h>
#include "Arduino.h"            // for 'boolean' and 'byte'


/**  Define 1 to hunt/search for RECORD, 0 for client to explicitly state offset. */
#define EEVALUES_CONF_HUNT_FOR_RECORD   01


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
#if EEVALUES_CONF_HUNT_FOR_RECORD
     boolean  findHeader();
#endif

     boolean  isHeaderValid(void);
     int      write(void);

     //  Erase our header and "user data portion".
     void eraseWholeRecord( uint8_t fill_value = 0xff );
     //  Erase the "user data portion" of the record.  0xFF reduces wear on EE memory.
     void eraseEeUserData( uint8_t fill_value = 0xff );

     //  Erase the EeValues' header in EE memory.
     void eraseEeHeader(void);

     EeIdent  ident(void) const { return m_header.m_ident; }

     //  User data exists in RAM memory ( not PROGMEM ).
     void     setUserDataPtr( void * user_data ) { m_user_data = user_data; }
     void *   userDataPtr(void) const { return m_user_data; }

     // Pass in size of user record ( that doesn't count EeValues header ).
     void     setUserSize( uint8_t siz ) { m_header.m_full_size = siz + sizeof(EeHeader); }

     //  Portion that is client's, i.e. after the EeHeader stored in front of record.
     //  Client's portion of record, not the actual size in ee-memory.
     unsigned userRecordSize(void) const { return m_header.m_full_size - sizeof(EeHeader); }

     //  Actual size stored, which includes EeValues overhead.
     unsigned totalSize(void) const { return m_header.m_full_size; }
     
     //  Return total size of EEMEM, i.e. E2END + 1.
     static unsigned    eeSize() ;

     //  After calling updateCrc8(), here writes from setUserDataPtr() into EE.
     int        writeToEe( void );

     //  Copies from EE into setUserDataPtr() using size in EE header.
     int        readToUser( void );

     //  Copies from EE into setUserDataPtr() using size in EE header.
     //   'ee_offset' is offset in all of EE, so use eeOffset() to find starting place
     //   of header to current record.
     int        readToUser( eeoffset_t ee_offset, void * user_buffer, size_t ee_count );

     void     updateCrc8();
     uint8_t  crc8() const   { return m_header.m_crc8; }
     void     setCrc8( uint8_t crc ) { m_header.m_crc8 = crc; }

     boolean     setEeOffset( eeoffset_t starting ) { m_start_offset = starting; }

     //  Return EE offset of where header was found.
     eeoffset_t  eeOffsetOfHeader(void) const { return m_start_offset; }
     eeoffset_t  eeOffsetOfUserRecord(void) const { return m_start_offset + sizeof(EeHeader); }

     //  Return EE-memory offset past last read or write.
     eeoffset_t  lastStoredOffset(void) { return m_start_offset + m_header.m_full_size; }

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

     //  Offset to start of header for user's record.
     eeoffset_t     m_start_offset;

     //  Ptr to application's RAM user data that connects to EEMEM via 'm_start_offset'.
     void *         m_user_data;

#if EEVALUES_CONF_HUNT_FOR_RECORD
     int       _find_ident();
#endif

   private :
     // no implementation for these:
     EeValues( const EeValues & );
     EeValues& operator=( const EeValues & );
     unsigned char operator==( const EeValues & rhs ) const;
};


#endif
