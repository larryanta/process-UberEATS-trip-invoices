#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

// The input and output file names are allowed to be this long:
#define MAXFNAMELEN 100

// The biggest CSV line we can handle:
#define MAXCSVLINE 200

// Maximum size of the report date string:
#define MAXDATESIZE 50




int main(int argc, char *argv[]) {
  char inFileName[MAXFNAMELEN+5];
  char outFileName[MAXFNAMELEN+5];
  FILE *csvFile;
  FILE *summaryFile;
  char *resultp;
  char buffer[200];
  char *p;
  char *q;
  int i, j;
  char charNet[15];
  char charHst[15];
  char charGross[15];
  unsigned long int net;
  unsigned long int hst;
  unsigned long int gross;
  char reportHeaderLine[200];
  char reportTaxYear[5];
  char reportDate[MAXDATESIZE];
  unsigned int countInvoicesAll=0;
  unsigned int countInvoicesWithNonZeroHst=0;
  unsigned long int sumNetAmtsAll=0;
  unsigned long int sumNetAmtsWithNonZeroHst=0;
  unsigned long int sumHst=0;
  unsigned long int sumGrossAmtsAll=0;
  unsigned long int sumGrossAmtsWithNonZeroHst=0;


  puts("Generating report 3...");




  // Handle command line arguments.
  if ( argc != 5 ) {
    printf("Usage: %s taxYear 'report date' inputFilename outputFilename\n(The date must be enclosed in single quotes.)\n", argv[0]);
    return 1;
  }


  if ( strlen(argv[1]) != 4 ) {
    puts("Tax year must be 4 digits.  Aborting.");
    return 2;
  }
  else
    strcpy(reportTaxYear,argv[1]);


  if ( strlen(argv[2]) > MAXDATESIZE ) {
    puts("Report date string too long.  Aborting.");
    return 3;
  }
  else
    strcpy(reportDate,argv[2]);


  if ( (reportDate[0]!='\'') || (reportDate[strlen(reportDate)-1]!='\'') ) {
    printf("%s\nReport date must be enclosed in single quotes.  Aborting.\n", reportDate);
    return 4;
  }


  j=strlen(reportDate)-2;   // Remove enclosing single quotes in report date.
  for ( i=0; i<j; i++ )
    reportDate[i] = reportDate[i+1];
  reportDate[i]='\0';


  if ( strlen(argv[3]) > MAXFNAMELEN ) {
    puts("Input file name too long.  Aborting.");
    return 5;
  }
  else
    strcpy(inFileName,argv[3]);


  if ( strlen(argv[4]) > MAXFNAMELEN ) {
    puts("Output file name too long.  Aborting.");
    return 6;
  }
  else
    strcpy(outFileName,argv[4]);




  // Open the input and output files.
  csvFile=fopen(inFileName,"r");
  if (!csvFile) {
    printf("Opening of CSV file %s failed.  Aborting.\n",inFileName);
    return 7;
  }


  summaryFile=fopen(outFileName,"w");
  if (!summaryFile) {
    printf("Opening of summary report file %s failed.  Aborting.\n",outFileName);
    return 8;
  }


  // Throw away the header line in the CSV file.
  resultp = fgets(buffer,MAXCSVLINE-5,csvFile);
  if ( ferror(csvFile) ) {
    printf("Error reading CSV file %s. Aborting.\n",inFileName);
    return 9;
  }


  // Process all remaining lines of the CSV file.
  resultp = fgets(buffer,MAXCSVLINE-5,csvFile);
  while (resultp) {
        if ( buffer[strlen(buffer)-1] == '\n' )  // If a newline is present,
          buffer[strlen(buffer)-1] = '\0';       //   replace it with a nul.

        // Position a pointer at the start of the net amount (one byte past the third-last comma).
        p=buffer+strlen(buffer);     // point to the last byte of the line (the nul byte)
        while ( *p-- != ',' )
          ;
        p--;
        while ( *p-- != ',' )
          ;
        p--;
        while ( *p-- != ',' )
          ;
        p++;
        p++;

        // Grab the net amount.  (Go forward to the second-last comma.)
        i=0;
        while ( *p != ',' )
          charNet[i++]=*p++;
        charNet[i]='\0';
        p++;


        // Grab the HST.  (Go forward to the last comma.)
        i=0;
        while ( *p != ',' )
          charHst[i++]=*p++;
        charHst[i]='\0';
        p++;


        // Grab the gross amount.  (Go forward to the nul.)
        i=0;
        while ( *p != '\0' )
          charGross[i++]=*p++;
        charGross[i]='\0';

        // For 100% accuracy let's avoid floating point values.  Get
        // rid of the decimal point in the net amount so we can work
        // with pennies.
        p = charNet;
        while ( *p++ != '.' )
          ;
        while ( *p != '\0' ) {
          *(p-1) = *p;
          p++;
        }
        p--;
        *p='\0';

        // Do the same for the HST amount.
        p = charHst;
        while ( *p++ != '.' )
          ;
        while ( *p != '\0' ) {
          *(p-1) = *p;
          p++;
        }
        p--;
        *p='\0';

        // And for the gross amount.
        p = charGross;
        while ( *p++ != '.' )
          ;
        while ( *p != '\0' ) {
          *(p-1) = *p;
          p++;
        }
        p--;
        *p='\0';

        // Convert the net amount string to an integer.
        errno=0;
        net = strtoul(charNet,&q,10);
        if ( (errno==ERANGE) || (*q!='\0') ) {
          printf("%s\nFormat of net amount value in above line is incorrect.  Aborting.\n",buffer);
          return 10;
        }

        // Do the same for the HST amount.
        errno=0;
        hst = strtoul(charHst,&q,10);
        if ( (errno==ERANGE) || (*q!='\0') ) {
          printf("%s\nFormat of HST value in above line is incorrect.  Aborting.\n",buffer);
          return 11;
        }

        // And the same for the gross amount.
        errno=0;
        gross = strtoul(charGross,&q,10);
        if ( (errno==ERANGE) || (*q!='\0') ) {
          printf("%s\nFormat of gross amount value in above line is incorrect.  Aborting.\n",buffer);
          return 12;
        }

        // Update running totals.
        countInvoicesAll++;
        sumNetAmtsAll   += net;
        sumGrossAmtsAll += gross;
        if (hst) {
          countInvoicesWithNonZeroHst++;
          sumNetAmtsWithNonZeroHst   += net;
          sumHst                     += hst;
          sumGrossAmtsWithNonZeroHst += gross;
        }

        resultp = fgets(buffer,MAXCSVLINE-5,csvFile);
  }



  // Make sure that the last fgets() didn't result in a file error.
  if ( ! feof(csvFile) ) {
    printf("Error reading CSV file %s. Aborting.\n",inFileName);
    return 13;
  }











  // Output the summary report.
  strcpy(reportHeaderLine,"Trip invoice summary for tax year ");
  strcat(reportHeaderLine,reportTaxYear);
  strcat(reportHeaderLine,"        Report date: ");
  strcat(reportHeaderLine,reportDate);

  if (!fprintf(summaryFile, "%s\n",reportHeaderLine)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "=======================================================================================\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "\n%u trip invoices were found for this tax year.\n", countInvoicesAll)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "%u of them had HST applied.\n", countInvoicesWithNonZeroHst)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "\nTotals for ALL trip invoices\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "============================\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "     Net: $ %9.2f\n", (float)sumNetAmtsAll/100.0)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "Plus HST: $ %9.2f\n", (float)sumHst/100.0)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "          ===========\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "   Total: $ %9.2f\n", (float)sumGrossAmtsAll/100.0)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "\nTotals for ONLY the trip invoices that have HST applied\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "=======================================================\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "     Net: $ %9.2f\n", (float)sumNetAmtsWithNonZeroHst/100.0)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "Plus HST: $ %9.2f\n", (float)sumHst/100.0)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "          ===========\n")) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }
  if (!fprintf(summaryFile, "   Total: $ %9.2f\n", (float)sumGrossAmtsWithNonZeroHst/100.0)) {
    puts("Error writing to summary file.  Aborting.");
    return 14;
  }


  fclose(csvFile);
  fclose(summaryFile);
  return 0;
}
