/*==================================================================== 
processUberEatsTripInvoices:  Extract dollar amounts from UberEATS pdf
                              trip invoices

Copyright (C) 2021  Larry Anta


You shouldn't have to modify anything in this program.  You can
compile it if you want, or let the invoking script compile it
automatically.

Sample build:

    gcc -lz -o rpt1pgm rpt1pgm.c
======================================================================*/




/*====================================================================
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
======================================================================*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zlib.h"

#define TRUE 1
#define FALSE 0
#define MAXINVOICENAME    200
#define MAXREPORTFILENAME 200
#define COMPRESSION_FACTOR_FOR_ZLIB 20

/* Global variables */
char invoiceName[MAXINVOICENAME];
char report1Filename[MAXREPORTFILENAME];

/* Function prototypes */
int ascii85decode(char *streamIn, unsigned long int inLen,
                  char *streamOut, unsigned long int *actualOutCount);




int main(int argc, char *argv[]) {

  /*=======================================================
  Extract the raw text from the UberEATS trip invoice given
  on the command line (a PDF file) and append that text to
  the report file whose name is also given on the command
  line.  (Then append a row of equal signs to the report
  file to separate this invoice from others.)

  Build with -lz switch to provide access to zlib.
  =========================================================*/


  FILE *invFile, *rptFile;
  unsigned long int charCount;
  int ch;
  int rc;               /* return code */
  unsigned int zCount;  /* number of ascii 'z' characters in the ascii85 stream */
  char *p;
  char *startp, *endp;
  char *wholeInvBuffer; /* points to an entire invoice in its original form */
  char *ascii85InBuff;  /* points to the ascii85 stream that was extracted from the invoice */
  char *ascii85OutBuff; /* points to the output area to which the decoded ascii85 stream goes */
  unsigned long int ascii85InBuffLen;
  unsigned long int ascii85OutBuffLen;
  unsigned long int ascii85ActualOutLen; /* actual number of bytes that ascii85decode() produced */

  /* zlib-related variables: */
  static const char *myZLIB_Version = ZLIB_VERSION;
  unsigned char     *inflateInBuff;
  unsigned char     *inflateOutBuff;
  unsigned long int  inflateInBuffSize;
  unsigned long int  inflateOutBuffSize;
  unsigned long int  inflateActualOutSize;
  z_stream d_stream; /* the decompression stream structure for zlib's inflate() */


  /*=====================================
  Capture the two command line arguments.
  =======================================*/
  if ( argc!=3 ) {
    printf("Usage: %s invoiceName report1Filename\n", argv[0]);
    return 1;
  }


  if ( strlen(argv[1]) > (MAXINVOICENAME-1) ) {
    printf("Invoice name too long.  Aborting.\n");
    return 2;
  }
  else strcpy(invoiceName,argv[1]);


  if ( strlen(argv[2]) > (MAXREPORTFILENAME-1) ) {
    printf("Report1's file name too long.  Aborting.\n");
    return 3;
  }
  else strcpy(report1Filename,argv[2]);


  /*==================================================
  With the way we've implemented the ascii85decode()
  function, we'll need 64-bit unsigned long long ints.
  Abort if they're shorter than that.
  ====================================================*/
  if ( sizeof(unsigned long long int) < 8 ) {
    printf("The ascii85decode() algorithm, as implemented here, "
           "needs 64-bit unsigned integers.  Aborting");
    return 4;
  }


  /*=====================================================
  Confirm that a compatible version of zlib is available.
  =======================================================*/
  if (zlibVersion()[0] != myZLIB_Version[0]) {
    printf("rpt1pgm: incompatible zlib version.  Aborting.\n");
    printf("rpt1pgm: zlib version %s = 0x%04x, compile flags = 0x%lx\n",
           ZLIB_VERSION, ZLIB_VERNUM, zlibCompileFlags());
    return 5;
  }
  else if (strcmp(zlibVersion(), ZLIB_VERSION) != 0) {
    printf("rpt1pgm: warning: different zlib version\n");
    printf("rpt1pgm: zlib version %s = 0x%04x, compile flags = 0x%lx\n",
           ZLIB_VERSION, ZLIB_VERNUM, zlibCompileFlags());
  }


  /*=====================================================================================
  The invoice is expected to be a PDF file with only one stream in it.

  Everything between the characters 'stream' and 'endstream' is expected to be printable
  PDF control and formatting objects that were first compressed into binary with the
  zlib compression library and then converted to ascii85 format (aka Base85 format).

  In other words, the filter order in the PDF is expected to say "/ASCII85decode" followed
  by "/FlateDecode" (but we don't check for those filter directives).

  We start by copying the whole PDF file into memory and isolating its only stream.  We
  run the stream through an ascii85 decoder, our home-grown ascii85decode() function, and
  then uncompress the result using zlib's inflate() library function.  (The zlib library
  is found in libz.so, so we'll need the -lz linking switch during program build.)

  The result will be printable PDF control and formatting objects which themselves
  contain the actual text that you'd see on paper if you printed the invoice.  We need
  to find and output those text tidbits to our report.  We do so in the physical order
  we find them in these PDF control and formatting objects without regard to where
  they'd actually be found on the printed page.
  =======================================================================================*/


  /*========================================================
  Open the PDF file and copy it into memory.  Then close the
  on-disk version since we'll work with it in memory.
  ==========================================================*/
  invFile=fopen(invoiceName,"r");
  if ( !invFile ) {
    printf("rpt1pgm: Error opening file %s for reading.  Aborting.\n",invoiceName);
    return 6;
  }
  charCount = 0LU;
  while ((ch=getc(invFile)) != EOF)
    charCount++;
  charCount++; /* one extra for a terminating nul */
  wholeInvBuffer = calloc(charCount,1);
  if (!wholeInvBuffer) {
    printf("Failed to allocate %lu bytes for invoice buffer.  Aborting.\n", charCount);
    return 7;
  }
  p=wholeInvBuffer;
  rewind(invFile);
  while ((ch=getc(invFile)) != EOF)
    *p++=ch;
  fclose(invFile);


  /*==========================================================
  Scan the invoice for a line that begins with the characters
  "stream".  Point to the 's'.  The word "stream" may appear
  in multiple places in the PDF file, but we're only
  interested in the one at the beginning of a line (hence we
  search for "\nstream").

  Also point to the character immediately preceding the word
  "endstream".
  ============================================================*/
  startp=strstr(wholeInvBuffer,"\nstream");
  if (!startp) {
    printf("Can't find start of stream in invoice.  Aborting.\n");
    return 8;
  } 
  startp++; /* step over the newline */

  endp=strstr(startp,"endstream");
  if (!endp) {
    printf("Can't find end of stream in invoice.  Aborting.\n");
    return 9;
  } 
  endp--;


  /*========================================================
  Fine-tune the start pointer so that it points to the first
  non-whitespace character following the word "stream".  The
  PDF specification is particular about what constitutes
  a whitespace character.
  ==========================================================*/
  startp += 6;   /* the byte following the 'm' */
  while (startp <= endp) {
    if (    *startp=='\0'
         || *startp=='\t'
         || *startp=='\n'
         || *startp=='\f'
         || *startp=='\r'
         || *startp==' ' )
      startp++;
    else
      break;
  }
  if (startp > endp) {
    printf("Couldn't find start of ascii85 stream.  Aborting.\n");
    return 10;
  }


  /*=====================================================
  We now know exactly where the ascii85 stream begins and 
  ends within the invoice.  Copy the stream to its own
  buffer and free the memory that the invoice occupied.

  While we're copying it, keep track of how many ascii 'z'
  characters we see since we'll need that total later.
  =======================================================*/
  ascii85InBuffLen = (endp - startp) + 1;
  ascii85InBuff = calloc(ascii85InBuffLen,1);
  if (!ascii85InBuff) {
    printf("rpt1pgm: Failed to allocate %lu bytes for ascii85 "
           "input buffer.  Aborting.\n", ascii85InBuffLen);
    return 11;
  }
  p = ascii85InBuff;
  zCount = 0;
  while (startp <= endp) {
    if ( *startp == 'z' )
      zCount++;
    *p++ = *startp++;
  }
  free(wholeInvBuffer);


  /*====================================================================
  Before decoding the ascii85 input stream with our ascii85decode()
  function, we need to create a buffer to hold that function's output.

  Other than ascii 'z's, every group of 5 input bytes will result in 4
  output bytes.  But every 'z' character in the input stream will
  expand to 4 bytes in the output stream.

  To be on the safe side, we'll make the size of the output buffer the
  same size as the input buffer plus 3 times the number of 'z's we saw
  in the input stream, thus guaranteeing that the output buffer will be
  big enough.

  Decode the ascii85 stream.  Notice that the function ascii85decode()
  will set the actual number of bytes that it sent to the output buffer.

  Upon return we no longer need the input buffer; so we free it.
  ======================================================================*/
  ascii85OutBuffLen = ascii85InBuffLen + 3 * zCount;
  ascii85OutBuff = calloc(ascii85OutBuffLen,1);
  if (!ascii85OutBuff) {
    printf("rpt1pgm: Failed to allocate %lu bytes for ascii85 "
           "output buffer.  Aborting.\n", ascii85OutBuffLen);
    return 12;
  }
  ascii85ActualOutLen=0;
  rc = ascii85decode(ascii85InBuff, ascii85InBuffLen, ascii85OutBuff, &ascii85ActualOutLen);
  if (rc) {
    printf("rpt1pgm: ascii85decode() returned error code %d.  Aborting.\n", rc);
    return 13;
  }
  free(ascii85InBuff);


  /*=========================================================================
  We're now ready to use the inflate() zlib library funtion to decompress
  the output of ascii85decode().  This will reveal the printable PDF control
  and formatting objects (that themselves contain the actual text you'd see
  on paper if you were to print the invoice or if you were to open the
  invoice with a PDF reader).

  The input to inflate() will be the buffer created by the ascii85decode()
  function and the length of that buffer.

  Start by giving them names that are more meaningful to the current zlib
  context.
  ===========================================================================*/
  inflateInBuffSize = ascii85ActualOutLen;
  inflateInBuff     = (unsigned char *)ascii85OutBuff;


  /*============================================================================
  Allocate a buffer for inflate()'s output.  Make it COMPRESSION_FACTOR_FOR_ZLIB
  times bigger than the input buffer so that there will certainly be enough room
  for inflate() to store its output in only one invocation.  This is probably a
  non-standard use of zlib's inflate() function but it results in simpler code.
  ==============================================================================*/
  inflateOutBuffSize = COMPRESSION_FACTOR_FOR_ZLIB * inflateInBuffSize;
  inflateOutBuff     = calloc((uInt)inflateOutBuffSize,1);
  if ( !inflateOutBuff ) {
    printf("rpt1pgm: Failed to allocate %lu bytes for inflate()'s output buffer."
           "  Aborting.\n", inflateOutBuffSize);
    return 14;
  }


  /*========================================================================
  Before actually invoking inflate(), we need to initialize the zlib inflate
  state with inflateInit().

  The d_stream structure is used to pass information to and from the zlib
  library functions.  The avail_in and next_in structure members are set in
  such a way as to indicate that no actual input data is being provided yet.
  ==========================================================================*/
  d_stream.zalloc   = Z_NULL;
  d_stream.zfree    = Z_NULL;
  d_stream.opaque   = Z_NULL;
  d_stream.avail_in = 0;
  d_stream.next_in  = Z_NULL;
  rc = inflateInit(&d_stream);
  if ( rc != Z_OK ) {
    printf("rpt1pgm: zlib function inflateInit() returned %d.  Aborting.\n", rc);
    return 15;
  }


  /*=======================================================================
  Hopefully we made COMPRESSION_FACTOR_FOR_ZLIB large enough so that zlib's
  inflate() function will consume all its input and produce all its output
  in just one call.  If that's the case, then inflate() should return
  Z_STREAM_END.

  Capture the actual number of bytes that inflate() placed in the output
  buffer.
  =========================================================================*/
  d_stream.next_in   = inflateInBuff;
  d_stream.avail_in  = inflateInBuffSize;
  d_stream.next_out  = inflateOutBuff;
  d_stream.avail_out = inflateOutBuffSize;
  rc = inflate(&d_stream, Z_NO_FLUSH);
  if (rc != Z_STREAM_END) {
    printf("rpt1pgm: Unexpected return code (%d) from inflate().  Aborting.\n", rc);
    (void)inflateEnd(&d_stream); /* to free memory */
    return 16;
  }
  inflateActualOutSize = d_stream.total_out;


  /*===================================================
  Tell zlib we're all done so it can free up any memory
  it allocated internally on our behalf.

  Also, we can now free up inflate()'s input buffer.
  =====================================================*/
  rc = inflateEnd(&d_stream);
  if ( rc != Z_OK ) {
    printf("rpt1pgm: zlib function inflateEnd() returned %d."
           "  Aborting.\n", rc);
    return 17;
  }
  free(inflateInBuff);


  /*==============================================
  We're ready to append the text from this invoice
  to the report file.  Open the report file.
  ================================================*/
  rptFile=fopen(report1Filename,"a");
  if ( !rptFile ) {
    printf("rpt1pgm: Error opening file %s for appending.  Aborting.\n",report1Filename);
    return 18;
  }


  /*=================================================================
  Use a Finite State Automaton (FSA) to traverse inflate()'s output
  buffer.

  A given line in that buffer may contain zero, one, or more pairs
  of matching brackets.  Characters that are enclosed within brackets
  are kept; others are discarded.  When a newline is encountered in
  the input, output a newline, but only if that line contained at
  least one pair of brackets.

  All brackets are assumed to be matched.  No check is made for this.

  Within a given pair of matched brackets, if a backslash ('\') is
  encountered, only output the character immediately following it.
  ===================================================================*/
#define START  1
#define END   99
int state;

  p=(char *)inflateOutBuff;
  endp=(char *)inflateOutBuff+inflateActualOutSize;
  state = START;
  while ( state != END ) {
        switch (state) {
          case START:    state=2;
                         break;

          case     2:    if ( p>endp ) {     /* "EOF" */
                           state=END;
                           break;
                         }
                         if ( *p=='(' ) {
                           p++;
                           state=3;
                           break;
                         }
                         p++;
                         break;

          case     3:    if ( *p==')' ) {
                           p++;
                           state=4;
                           break;
                         }
                         if ( *p=='\\' ) {
                           fputc(*(p+1),rptFile);
                           p+=2;
                           break;
                         }
                         fputc(*p,rptFile);
                         p++;
                         break;

          case     4:    if ( *p=='(' ) {
                           p++;
                           state=3;
                           break;
                         }
                         if ( *p=='\n' ) {
                           fputc('\n',rptFile);
                           p++;
                           state=2;
                           break;
                         }
                         p++;
                         break;
        }
  }
  fprintf(rptFile,"================================================================="
                  "===============================\n");


  /*====================================
  Normal return of control to our caller
  ======================================*/
  free(inflateOutBuff);
  fclose(rptFile);
  return 0;
} /* main() */














/*======================
Function ascii85decode()
========================*/
int ascii85decode(char *streamIn, unsigned long int inLen,
                  char *streamOut, unsigned long int *actualOutCount) {

  /* Successive powers of 85 */
  #define EXP0_85        1ULL
  #define EXP1_85       85ULL
  #define EXP2_85     7225ULL
  #define EXP3_85   614125ULL
  #define EXP4_85 52200625ULL

  int i;
  int wsCounter;             /* white space counter */
  int bytesLeft;
  unsigned long long int sum;
  unsigned long long int b1, b2, b3, b4;
  unsigned long long int residVal;
  char *tempBuff;
  char *p, *q;
  char shortGroup[5];

  /*==========================================================================
  Ascii85 encoding (aka Base85 encoding) translates arbitrary binary data into
  printable ascii characters.  Since there are 256 possible bit patterns in an
  8-bit byte but far fewer printable ascii characters, every 4 bytes of binary
  data results in 5 printable ascii characters.  After outputting all the
  printable ascii characters, ascii85 appends an end-of-data (EOD) marker, the
  two characters '~>'.

  Ascii85decode (this function) reverses this process by converting each group
  of 5 printable ascii characters back into their original 4-byte binary form.

  Ascii85decode ignores all white-space characters it encounters.  The PDF
  specification defines these six white-space characters:

    Decimal  Hexadecimal        Details
    -------  -----------  ---------------------
       0         00       '\0'  Null (NUL)
       9         09       '\t'  Tab (HT)
      10         0A       '\n'  Line feed (LF)
      12         0C       '\f'  Form feed (FF)
      13         0D       '\r'  Carriage return (CR)
      32         20       ' '   Space (SP)

  As we traverse the stream of printable ascii characters to be decoded from
  left to right, the general case is to gather the next 5 ascii characters in
  a row and to process them as a group.  If the stream was encoded correctly,
  each of the 5 ascii characters is guaranteed to be in the range 33 through
  117 (ascii character '!' through 'u').

  There is one exception: it could also have the value 122 (ascii character
  'z').  We'll discuss what to do with zs separately, as well as what to do
  when the very last group contains less than 5 ascii characters.

  For the general case, we start by subtracting 33 from each of the 5 ascii
  characters.  (Call them c1, c2, c3, c4 and c5.)  For example, if the 5 ascii
  characters are '6M<G#' then:

      c1 is   6   starts as ascii 54; less 33, c1 becomes 21
      c2 is   M   starts as ascii 77; less 33, c2 becomes 44
      c3 is   <   starts as ascii 60; less 33, c3 becomes 27
      c4 is   G   starts as ascii 71; less 33, c4 becomes 38
      c5 is   #   starts as ascii 35; less 33, c5 becomes  2

  We then compute: (c1 * 85**4) + (c2 * 85**3) + (c3 * 85**2) + (c4 * 85) + c5

  In other words, we treat the 5 consecutive values as a base-85 number.
  In our example, we get the total 1,123,432,932 as follows:

           21 * 85**4        21 * 52,200,625      1,096,213,125
           44 * 85**3        44 *    614,125         27,021,500
           27 * 85**2        27 *      7,225            195,075
           38 * 85**1        38 *         85              3,230
         +  2 * 85**0      +  2 *          1      +           2
         ============      =================      =============
                                                  1,123,432,932

  We then express this total as a base-256 number by computing b1, b2, b3 and
  b4 in the following expression:

       1,123,432,932 = (b1 * 256**3) + (b2 * 256**2) + (b3 * 256) + b4

  In our example, this would give us:

          66 * 256**3        66 * 16,777,216      1,107,296,256
         246 * 256**2       246 *     65,536         16,121,856
          57 * 256**1        57 *        256             14,592
       + 228 * 256**0     + 228 *          1      +         228
       ==============     ==================      =============
                                                  1,123,432,932

  The coefficients of the base-256 number would therefore be:

      b1:    66 (hexadecimal 42)
      b2:   246 (hexadecimal f6)
      b3:    57 (hexadecimal 39)
      b4:   228 (hexadecimal e4)

  So, while ascii85 encoded the 4-byte hexadecimal value 0x42f639e4 into
  the 5 ascii characters '6M<G#', ascii85decode (this function) would be
  used to translate '6M<G#' back into 0x42f639e4.

  The above is the normal processing.  However, when ascii85 is encoding
  4 consecutive bytes of binary 0s (0x'00000000'), rather than outputting
  5 ascii characters (which would be '!!!!!'), it outputs only 1 ascii
  character, a 'z' (decimal value 122).

  Therefore, when a 'z' is encountered by asci885decode (this function), we
  immediately output 0x'00000000' and treat the character after the 'z' in
  the input stream as potentially the first character in the next group of
  5 characters (although it could be another 'z' of course).  (A properly
  encoded input stream will never have a 'z' in the middle of a group of 5
  characters.)

  The only other consideration is what we should do with the final group
  of bytes in the input stream if that group has less than 5 characters
  in it.  Here's how the official PDF specification describes the encoding
  of short groups by ascii85 (the encoding process):

      "If the length of the binary data to be encoded is not
      a multiple of 4 bytes, the last, partial group of 4 is
      used to produce a last, partial group of 5 output
      characters.  Given n (1, 2, or 3) bytes of binary data,
      the encoder first appends 4-n zero bytes to make a
      complete group of 4.  It then encodes this group in the
      usual way, but without applying the special z case.
      Finally, it writes only the first n+1 characters of the
      resulting group of 5."

  Since n+1 ascii bytes are output by the encoder in this case, we should
  never see a final group with only 1 ascii character; it must have m (2, 3,
  or 4) ascii characters.  To reproduce the original binary data, we append
  5-m ascii 'u' characters, do our normal processing, and output only the
  first m-1 binary bytes.
  ============================================================================*/










  /*=====================================================================
  Ensure that the required 2-byte EOD marker ('~>') is present at the end
  of the stream and reduce the stream length by 2 to account for it.
  =======================================================================*/
  if (   (*(streamIn+inLen-2)!='~')  ||  (*(streamIn+inLen-1)!='>')   ) {
    printf("EOD ('~>') missing at end of stream.  Aborting.\n");
    return 1;
  }
  inLen -= 2;


  /*=============================================================================
  Remove any whitespace characters in the input stream by:
    (1) -creating a temporary buffer that's the same size as the input stream
    (2) -copying only the non-whitespace characters to the temporary buffer
    (3) -reducing the stream length by the number of whitespace characters we saw
    (4) -copying the temporary buffer back into the input stream
    (5) -destroying the temporary buffer
  ===============================================================================*/
  tempBuff = calloc(inLen,1);
  if (!tempBuff) {
    printf("ascii85decode(): Failed to allocate %lu bytes for temp buffer.  Aborting.\n", inLen);
    return 2;
  }
  p=streamIn;
  q=tempBuff;
  wsCounter = 0;
  for ( i=0; i<inLen; i++ ) {
    switch (*p) {
      case '\0':
      case '\t':
      case '\n':
      case '\f':
      case '\r':
      case  ' ':  wsCounter++;
                  p++;
                  break;
      default:    *q++=*p++;
                  break;
    }
  }
  inLen -= wsCounter;
  p=streamIn;
  q=tempBuff;
  for ( i=0; i<inLen; i++ ) {
    *p++=*q++;
  }
  free(tempBuff);


  /*==================================================
  Decode the input stream, keeping track of the number
  of bytes we write to the output stream.
  ====================================================*/
  p=streamIn;
  q=streamOut;
  bytesLeft=inLen;
  *actualOutCount = 0;
  while ( (bytesLeft>=5)  ||  (*p=='z')  ) {
      if ( *p == 'z' ) {
        *q++=0x0;
        *q++=0x0;
        *q++=0x0;
        *q++=0x0;
        (*actualOutCount) += 4;
        p++;
        bytesLeft--;
        continue;
      }
      sum = (  ((int)*(p+0)-33) * EXP4_85  )  +
            (  ((int)*(p+1)-33) * EXP3_85  )  +
            (  ((int)*(p+2)-33) * EXP2_85  )  +
            (  ((int)*(p+3)-33) * EXP1_85  )  +
            (  ((int)*(p+4)-33) * EXP0_85  );
      b1 = sum>>24;                    /* integer-divide             by 256**3  (2**24) (16,777,216) */
      residVal  = sum - (b1<<24);
      b2 = residVal>>16;               /* integer-divide what's left by 256**2  (2**16)     (65,536) */
      residVal  = residVal - (b2<<16);
      b3 = residVal>>8;                /* integer-divide what's left by 256**1   (2**8)        (256) */
      b4 = residVal - (b3<<8);
      *q++=b1;
      *q++=b2;
      *q++=b3;
      *q++=b4;
      (*actualOutCount) += 4;
      p += 5;
      bytesLeft -= 5;
  }


  /*=========================================
  Bytes left over?  The final group must be a
  short one. (All 'z's will have been taken
  care of above by the way.)
  ===========================================*/
  if (bytesLeft) {
      shortGroup[0]='u';
      shortGroup[1]='u';
      shortGroup[2]='u';
      shortGroup[3]='u';
      shortGroup[4]='u';
      for (i=0; i<bytesLeft; i++)
        shortGroup[i] = *(p+i);

      sum = (  ((int)(shortGroup[0])-33) * EXP4_85  )  +
            (  ((int)(shortGroup[1])-33) * EXP3_85  )  +
            (  ((int)(shortGroup[2])-33) * EXP2_85  )  +
            (  ((int)(shortGroup[3])-33) * EXP1_85  )  +
            (  ((int)(shortGroup[4])-33) * EXP0_85  );
      b1 = sum>>24;                    /* integer-divide             by 256**3  (2**24) (16,777,216) */
      residVal  = sum - (b1<<24);
      b2 = residVal>>16;               /* integer-divide what's left by 256**2  (2**16)     (65,536) */
      residVal  = residVal - (b2<<16);
      b3 = residVal>>8;                /* integer-divide what's left by 256**1   (2**8)        (256) */
      b4 = residVal - (b3<<8);
      switch (bytesLeft) {
        case 2: *q++=b1;
                (*actualOutCount)++;
                break;
        case 3: *q++=b1;
                *q++=b2;
                (*actualOutCount) += 2;
                break;
        case 4: *q++=b1;
                *q++=b2;
                *q++=b3;
                (*actualOutCount) += 3;
                break;
      }
  }


  return 0;
} /* ascii85decode() */
