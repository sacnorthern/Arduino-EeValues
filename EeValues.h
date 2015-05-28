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


/* ------------------------------------------------------------------- */

#define _EEVALUES_VERSION  1

/**  Define 1 to hunt/search for RECORD, 0 for client to explicitly state offset. */
#define _EEVALUES_CONF_HUNT_FOR_RECORD   0

#define _EEVALUES_CRC_SEED  0x81

/** Allow creation of 4 bytes that are 4 characters.  Arduino has 2 byte 'int' types.  LITTLE-ENDIAN version. */
#define MK4CODE(_a,_b,_c,_d)   ( ((uint32_t)(_d) << 24) | ((uint32_t)(_c) << 16) | ((_b) << 8) | (_a) )

typedef uint32_t  EeIdent;

/* ------------------------------------------------------------------- */

class EeValues
{
   public :
#if _EEVALUES_CONF_HUNT_FOR_RECORD   
     boolean  tryRead();
#endif
     boolean  tryRead( uint16_t offset );
     int      write(void);

     //  Erase the "user data portion" of the record.  0xFF reduces wear on EE memory.
     void eraseUserData( uint8_t fill_value = 0xff );

     EeIdent  ident(void) const { return m_header.m_ident; }
     // void    setIdent( EeIdent id ) { m_ident = id; }

     /*** Pass in size of whole record including EeValues header.
      *   @param siz - sizeof(WholeRecordSubClassedFromEeValues)
      */
     void     setSize( uint8_t siz ) { m_header.m_full_size = siz - sizeof(EeValues) + sizeof(EeHeader); }

     //  Actual size stored, which includes EeValues overhead.
     unsigned realSize() const { return m_header.m_full_size; }

     //  Portion that is client's, i.e. after the EeHeader stored in front of record.
     //  Client size of record, not the actual size in ee-memory.
     unsigned userSize() const { return m_header.m_full_size - sizeof(EeHeader); }

     //  Return EE-memory offset immediately after this whole record.
     //  Valid once a valid record has been detected.
     unsigned offsetAfter() const { return s_start_offset + realSize(); }
     
     void     updateCrc8();
     uint8_t  crc8() const   { return m_header.m_crc8; }
     void     setCrc8( uint8_t crc ) { m_header.m_crc8 = crc; }

     //  Return EE-memory offset of last read or write.
     static uint16_t  getLastStoreSffset(void) { return s_start_offset; }
     
   protected :
     //  Instance vars are stored ahead of client record.
     //  CRC IS STORED FIRST, WHEN COMPUTING CHECK, CODE ASSUMES THIS.
     struct EeHeader {
        uint8_t        m_crc8;             // valid after call to this->update_crc8()

        uint8_t        m_full_size;        // actual, full size, including EeValues overhead.

        EeIdent        m_ident;
     } m_header;

#if _EEVALUES_CONF_HUNT_FOR_RECORD
     int       _find_ident();
#endif

     uint8_t * _userDataPtr() const { return (uint8_t *) (this+1); }

     //  Static ( class-vars ) that are not stored in EE memory.
     static int  s_start_offset;
     
     //  Protected constructor so client MUST derive this class.
     //  Doing this inline saves 2 bytes (Sept 2013).
     EeValues( EeIdent id, size_t complete_siz )
     {
        // GCC 4.4 can't initialize a field inside an instance-var structure, so do it manually.
        m_header.m_ident = id;
        m_header.m_crc8 = 0;
        m_header.m_full_size = complete_siz - sizeof(*this) + sizeof(m_header);
     }

   private :
     // no implementation for these:
     EeValues( const EeValues & );
     EeValues& operator=( const EeValues & );
     unsigned char operator==( const EeValues & rhs ) const;
};
 
 
#endif
 