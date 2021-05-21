/*==================================================================== 
processUberEatsTripInvoices:  Extract dollar amounts from UberEATS pdf
                              trip invoices

Copyright (C) 2021  Larry Anta


You shouldn't have to modify anything in this program.  You can
compile it if you want, or let the invoking script compile it
automatically.

Sample build:

    gcc -o rpt2pgm rpt2pgm.c
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
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0


/*=========================================================================
Return
 Code     Meaning
------ --------------------------------------------------------------------
   0   -normal, no errors detected
   1   -could not open the raw text file
   2   -too many invoices
   3   -not enough memory to hold all trip invoices (malloc failure)
   4   -byte count mismatch between disk and in-memory copy of text file
   5   -could not find first byte of first invoice in memory
   6   -could not find last byte of first invoice in memory
   7   -could not find last byte of an invoice in memory
   8   -error opening CSV file
   9   -error writing to CSV file
  10   -invoice has no invoice number
  11   -invoice has no invoice date
  12   -invoice missing string 'Uber Portier B.V.'
  13   -invoice missing GST registration number
  14   -invoice missing net amount
  15   -invoice missing gross amount
  16   -error writing to CSV file
  17   -invalid command line arguments
  18   -input file name too long
  19   -output file name too long
  20   -no memory for first linked-list node
  21   -no memory for new linked-list node
=========================================================================*/


/*===================================================
How big is an unsigned long int?  Since we don't know
where this program will be compiled, let's assume the
worst case: that an unsigned long int is only 32 bits
wide.  This gives us a usable range of from 0 to
+4,294,967,295.
=====================================================*/
#define MAX_SIZE 4294967295


/*======================================================
Set aside this many bytes for the restaurant name, three
of which are for the enclosing double quotes and the nul
byte at the end.
========================================================*/
#define RESTAURANTMAX 53


/* The input and output file names are allowed to be this long: */
#define MAXFNAMELEN 100


/*==============================================
A linked-list.  Each node will hold the starting
and ending addresses of one invoice.
================================================*/
struct invoices {
  char            *firstByte;
  char            *lastByte;
  struct invoices *next;
};


/* A cleanup function to close files and free memory.  */
void cleanup(int code, FILE *raw, FILE *csv, char *buffp, struct invoices *llistp);




/* Mainline */
int main(int argc, char *argv[]) {
  FILE *rawTextFile;
  FILE *csvFile;
  unsigned long int charCountA, charCountB;
  char inFileName[MAXFNAMELEN+5];
  char outFileName[MAXFNAMELEN+5];
  int i, j;
  int ch;
  int invCount;
  char *startBufferp;  /* the start of the in-storage buffer containing the file */
  char *startp, *endp; /* the starting and ending address of an individual invoice */
  char *w, *x;         /* work pointers */
  char invNum[30];
  char invDate[20];
  char taxPointDate[20];
  char restaurantName[RESTAURANTMAX];
  char gstNumber[20];
  char netAmt[10];
  char grossAmt[10];
  char hstAmt[10];
  struct invoices *p,
                  *firstNode=NULL,
                  *prevNode=NULL;


  puts("Generating report 2...");


  /* Handle command line arguments. */
  if ( argc != 3 ) {
    printf("Usage: %s inputFilename outputFilename\n", argv[0]);
    return 17;
  }
  if ( strlen(argv[1]) > MAXFNAMELEN ) {
    puts("Input file name too long.  Aborting.");
    return 18;
  }
  else
    strcpy(inFileName,argv[1]);
  if ( strlen(argv[2]) > MAXFNAMELEN ) {
    puts("Output file name too long.  Aborting.");
    return 19;
  }
  else
    strcpy(outFileName,argv[2]);


  /* Open the raw text file. */
  rawTextFile=fopen(inFileName,"r");
  if (!rawTextFile) {
    puts("Opening raw text file failed.  Aborting.");
    cleanup(1,NULL,NULL,NULL,NULL);
    return 1;
  }


  /*=============================================
  Make sure the file isn't too big.  (Reserve one
  extra byte for the '\0' we're going to append.)
  ===============================================*/
  charCountA=0LU;
  while ((ch=getc(rawTextFile)) != EOF) {
    charCountA++;
    if (charCountA==(MAX_SIZE-1)) {
      puts("Too many invoices.  Aborting.");
      cleanup(2,rawTextFile,NULL,NULL,NULL);
      return 2;
    }
  }


  /*===========================================================
  Allocate enough space to hold the whole file in memory, plus
  one extra byte for the '\0' that we're going to append to it.
  =============================================================*/
  startBufferp=(char *)malloc(charCountA+1);
  if (!startBufferp) {
    printf("malloc() failed to allocate %lu bytes\nAborting.", charCountA+1);
    cleanup(3,rawTextFile,NULL,NULL,NULL);
    return 3;
  }


  /*================================================================
  Read the whole file into memory and append a '\0' byte to the end.
  The nul byte will stop string functions list strstr() from going
  past the last invoice.
  ==================================================================*/
  rewind(rawTextFile);
  charCountB=0LU;
  w=startBufferp;
  while ((ch=getc(rawTextFile)) != EOF) {
    charCountB++;
    *w++=ch;
  }
  if (charCountA!=charCountB) {
    puts("Byte count mismatch.  Possible error reading file into memory.  Aborting.");
    cleanup(4,rawTextFile,NULL,startBufferp,NULL);
    return 4;
  }
  *w='\0';


  /*====================================================
  We now have the whole file in memory and it's been
  nul-terminated.  The disk version is no longer needed.
  ======================================================*/
  fclose(rawTextFile);


  /*==========================================================
  The first invoice is preceded by a long row of equal signs.
  Every invoice is also FOLLOWED by a long row of equal signs,
  including the last one.

  To search for the beginning of an invoice, look for this
  string of characters: '===\nIssued on behalf of '.  All
  invoices start at the letter 'I' in the word 'Issued'.

  To find the end of a given invoice, look for this string of
  characters: '\n==='.  Each invoice ends at the character
  just before the newline.

  Let's now get the starting and ending addresses of the very
  first invoice.
  ============================================================*/
  w=strstr(startBufferp,"===\nIssued on behalf of ");
  if ( !w ) {
    puts("Couldn't find beginning of first invoice.  Aborting.");
    cleanup(5,NULL,NULL,startBufferp,NULL);
    return 5;
  }
  startp=w+4;          /* the 'I' in Issued */
  w=strstr(w,"\n===");
  if ( !w ) {
    puts("Couldn't find ending of first invoice.  Aborting.");
    cleanup(6,NULL,NULL,startBufferp,NULL);
    return 6;
  }
  endp=w-1;            /* the character just prior to the newline */


  /*================================================================
  We can now create the first node in the linked list.  There is one
  node for each invoice.  In each node, we store the starting and
  ending addresses of an invoice.
  ==================================================================*/
  p = (struct invoices *) malloc(sizeof(struct invoices));
  if ( !p ) {
    puts("No memory for first linked-list node.  Aborting.");
    cleanup(20,NULL,NULL,startBufferp,NULL);
    return 20;
  }
  p->firstByte = startp;
  p->lastByte  = endp;
  p->next      = NULL;
  firstNode = prevNode = p;


  /*============================================================
  Find all remaining invoices and add them to the linked-list.

  Our work pointer, w, was left pointing at the end of the first
  invoice.  Continue using w to find all the remaining invoices.
  (When there are no more to be found, w becomes NULL.)
  ==============================================================*/
  w=strstr(w,"===\nIssued on behalf of ");
  while (w) {
    startp=w+4;          /* the 'I' in Issued */
    w=strstr(w,"\n===");
    if ( !w ) {
      puts("Couldn't find the end of one of the invoices.  Aborting.");
      cleanup(7,NULL,NULL,startBufferp,firstNode);
      return 7;
    }
    endp=w-1;            /* the character just prior to the newline */
    p = (struct invoices *) malloc(sizeof(struct invoices));
    if ( !p ) {
      puts("No memory for new linked-list node.  Aborting.");
      cleanup(21,NULL,NULL,startBufferp,NULL);
      return 21;
    }
    p->firstByte = startp;
    p->lastByte  = endp;
    p->next      = NULL;
    prevNode->next = p;
    prevNode = p;
    w=strstr(w,"===\nIssued on behalf of ");
  }


  /* We're ready to create the CSV file.  Open it and output a header row. */
  csvFile=fopen(outFileName,"w");
  if (!csvFile) {
    puts("Error opening CSV file.  Aborting.");
    cleanup(8,NULL,NULL,startBufferp,firstNode);
    return 8;
  }
  if (!fprintf(csvFile,"InvoiceNumber,InvoiceDate,TaxPointDate,Restaurant,"
                       "GSTNumber,TotalNet,TotalHST,GrossAmt\n")) {
    puts("Error writing to CSV file.  Aborting.");
    cleanup(9,NULL,csvFile,startBufferp,firstNode);
    return 9;
  }




  /*=================================================================================
  There is one node per invoice in our linked-list.  Traverse the entire linked-list 
  and, for each invoice, extract the fields we need for the next row of the CSV file.
  Output that row.
  ===================================================================================*/
  invCount=0;
  p = firstNode;
  while (p) {


        startp = p->firstByte; /* the address of the first byte of this invoice */
        endp   = p->lastByte;  /* the address of the last byte of this invoice */


        /*===============================================================================
        Every invoice should have an invoice number.  It's on a line that begins with the
        text 'Invoice Number:  '.  (Note the two spaces after the colon.)  Scoop up the
        rest of the line and replace the '\n' at the end with a '\0'.
        =================================================================================*/
        x=strstr(startp,"\nInvoice Number:  ");
        if ( !x || (x>endp) ) {
          puts("\nInvoice found without an invoice number.  Aborting.");
          cleanup(10,NULL,csvFile,startBufferp,firstNode);
          return 10;
        }
        x+=strlen("\nInvoice Number:  ");
        w=invNum;
        while ( *x != '\n' ) {
          *w++ = *x++;
        }
        *w='\0';


        /*====================================================================
        Do the same for the invoice date but enclose the date in double quotes
        since it contains a comma.  (CSV-file rules require this.)
        ======================================================================*/
        x=strstr(startp,"\nInvoice Date:  ");
        if ( !x || (x>endp) ) {
          printf("\nInvoice %s does not have an invoice date.  Aborting.\n",invNum);
          cleanup(11,NULL,csvFile,startBufferp,firstNode);
          return 11;
        }
        x+=strlen("\nInvoice Date:  ");
        w=invDate;
        *w++='\"';
        while ( *x != '\n' )
          *w++ = *x++;
        *w++='\"';
        *w='\0';


        /*===================================================================
        The tax point date is a little tricky.  It's not always present.

        Sometime around March 15, 2021, Uber seems to have stopped putting
        tax point dates in trip invoices.  Even an invoice that contains
        the text 'Tax Point Date' doesn't necessarily have one.  (All
        invoices still contain an invoice date though.)

        I've noticed that invoices that actually do contain a tax point date
        also have a line that starts with 'Delivery service'.  In that case,
        the tax point date is the entire line immediately above the
        'Delivery service' line.

        As with the invoice date, the tax point date contains a comma, so we
        have to enclose it in double quotes.

        If an invoice does NOT contain a tax point date, we set the tax point
        date field to 'notSpecified' in the CSV file.
        =====================================================================*/
        strcpy(taxPointDate,"notSpecified");
        x=strstr(startp,"\nDelivery service");
        if ( x && (x<endp) ) {
          w=taxPointDate;
          *w++='\"';
          x--;
          while ( *x != '\n' )                    /* Back up to the previous newline... */
            x--;
          x++;
          while ( *x != '\n' )          /* ...and go forward again, capturing the date. */
            *w++ = *x++;
          w--;  /* There's always a blank at the end of the tax point date.  Remove it. */
          *w++='\"';
          *w='\0';
        }


        /*==========================================================
        Every invoice has a restaurant name.

        It's on the line following the line that starts with the
        text 'Uber Portier B.V.'.

        It's conceivable that a restaurant's name contains a comma,
        so, to be safe, we always enclose the name in double quotes.
        We also truncate the restaurant's name if it's too long.

        Pray that we never see a restaurant name with a double
        quote in it.  (We would need to modify this code to double
        each of those double quotes.)
        ============================================================*/
        x=strstr(startp,"\nUber Portier B.V.");
        if ( !x || (x>endp) ) {
          printf("\nInvoice %s does not contain 'Uber Portier B.V.'.  Aborting.\n",invNum);
          cleanup(12,NULL,csvFile,startBufferp,firstNode);
          return 12;
        }
        x++;
        while ( *x++ != '\n' )      /* Ignore the rest of this line. */
          ;
        /*======================================================
        Prepare to truncate the restaurant's name, if necessary.
        We need to reserve 3 bytes for the 2 enclosing double
        quotes and the string's terminating nul.
        ========================================================*/
        w=restaurantName;
        *w++='\"';
        j = RESTAURANTMAX - 3;    /* At most, copy this many bytes. */
        for ( i=0; i<j; i++ ) {
          if ( *x == '\n' )
            break;
          *w++ = *x++;
        }
        *w++='\"';
        *w='\0';


        /*=============================================================
        Every invoice has two GST registration numbers, one for the
        restaurant and one for the driver.

        The restaurant's GST number appears first and is on a line that
        starts with the text 'GST Registration Number: '.
        ===============================================================*/
        x=strstr(startp,"\nGST Registration Number: ");
        if ( !x || (x>endp) ) {
          printf("\nInvoice %s does not contain a GST registration number.  Aborting.\n",invNum);
          cleanup(13,NULL,csvFile,startBufferp,firstNode);
          return 13;
        }
        x+=strlen("\nGST Registration Number: ");
        w=gstNumber;
        while ( *x != '\n' )
          *w++ = *x++;
        *w++='\0';


        /*=======================================================
        Every invoice has a net amount.  It's the value found on
        the line following the line that contains only this text:
        'Total Net '.

        The value starts at the first byte of the line and is
        followed by at least one blank.
        =========================================================*/
        x=strstr(startp,"\nTotal Net \n");
        if ( !x || (x>endp) ) {
          printf("\nInvoice %s does not contain a net amount.  Aborting.\n",invNum);
          cleanup(14,NULL,csvFile,startBufferp,firstNode);
          return 14;
        }
        x+=strlen("\nTotal Net \n");
        w=netAmt;
        while ( *x != ' ' )
          *w++ = *x++;
        *w++='\0';


        /*========================================================
        Every invoice has a gross amount.  It's the value found on
        the line following the line that contains only this text:
        'Gross Amount '.

        The value starts at the first byte of the line and is
        followed by at least one blank.
        ==========================================================*/
        x=strstr(startp,"\nGross Amount \n");
        if ( !x || (x>endp) ) {
          printf("\nInvoice %s does not contain a gross amount.  Aborting.\n",invNum);
          cleanup(15,NULL,csvFile,startBufferp,firstNode);
          return 15;
        }
        x+=strlen("\nGross Amount \n");
        w=grossAmt;
        while ( *x != ' ' )
          *w++ = *x++;
        *w++='\0';


        /*=========================================================
        Not all invoices contain an HST amount.  If the invoice has
        a line that contains only the text 'Total HST Amount ',
        then the HST amount is on the line following that one.

        The HST starts at the first byte and is followed by at
        least one blank.

        If the invoice does not have an HST amount, we set the
        HST value to '0.00'.
        ===========================================================*/
        strcpy(hstAmt,"0.00");
        x=strstr(startp,"\nTotal HST Amount \n");
        if ( x && (x<endp) ) {
          x+=strlen("\nTotal HST Amount \n");
          w=hstAmt;
          while ( *x != ' ' )
            *w++ = *x++;
          *w++='\0';
        }


        /* We have all the fields we need.  Add a new row to the CSV file. */
        if (!fprintf(csvFile,"%s,%s,%s,%s,%s,%s,%s,%s\n",invNum,invDate,taxPointDate,restaurantName,
                                                         gstNumber,netAmt,hstAmt,grossAmt)) {
          puts("Error writing to CSV file.  Aborting.");
          cleanup(16,NULL,csvFile,startBufferp,firstNode);
          return 16;
        }


        invCount++;
        printf("%d ", invCount);
        p = p->next;           /* continue with next invoice */
  }
  puts("");
  cleanup(0,NULL,csvFile,startBufferp,firstNode);
  return 0;
}






void cleanup(int code, FILE *raw, FILE *csv, char *buffp, struct invoices *llistp) {
  /*==========================================================================
  There are many points in the mainline where an error is detected and control
  must be returned to the operating system.  Depending on where we are in our
  processing, there may be files we've opened that need to be closed or
  dynamic memory that we've allocated that needs to be freed.

  We're passed an action code to tell us what to clean up.  (Doing all the
  clean up in a function helps keep the mainline uncluttered.)
  ============================================================================*/

  struct invoices *p, *q;

  switch (code) {
    case 0:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16: fclose(csv);
             free(buffp);
             p=llistp;      /* free a linked-list */
             while (p) {
               q = p->next;
               free(p);
               p=q;
             }
             break;
    case 1:  break;         /* no action needed */
    case 2:
    case 3:  fclose(raw);
             break;
    case 4:  fclose(raw);
             free(buffp);
             break;
    case 5:
    case 6:
    case 20:
    case 21: free(buffp);
             break;
    case 7:
    case 8:  free(buffp);
             p=llistp;      /* free a linked-list */
             while (p) {
               q = p->next;
               free(p);
               p=q;
             }
             break;
  }
}
