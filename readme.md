Licensing
=========
See GPL file: LICENSE.txt

General Info
============

The processUberEatsTripInvoices korn shell script extracts dollar amounts from the
trip invoices (PDF files) that Uber Eats makes available to its drivers. This can
assist with GST/HST statement filings in Ontario and, perhaps, other provinces
in Canada. 

There are two lines that must be modifed in the script before you can use it.
(See the script itself for details.)

Also, before running the script, these packages must be present on your Linux
system:

     ksh
     python
     PyPDF2

The script invokes two C programs.  You can either compile them yourself
beforehand or let the script do it for you automatically.



Where do I find my UberEATS trip invoices?
==========================================

As an UberEATS delivery person you can find your trip invoices by logging into your
driver account at drivers.uber.com and clicking on the INVOICES link.

Each one is a PDF file.  Download them all to your Linx box.



Backgound (Do I even need this script?)
=======================================

As of March, 2021, Intuit, the makers of TurboTax, are of the opinion that food delivery
drivers do not remit HST to the CRA, but they are eligible to recover HST they have paid
on their own expenses by claiming Input Tax Credits (ITCs).  That's my reading of their
excellent article "GST/HST Reporting Requirements For Food Delivery Services" by Hebatollah
El-Kady, available here:

turbotax.intuit.ca/tips/gst-hst-reporting-requirements-for-food-delivery-services-11950

But Uber appears to be giving restaurants the GST registration numbers of its drivers.
Worse, they are charging the restaurant HST (at 13% here in Ontario) and stating right in
the invoice that the invoice was created on behalf of the driver.  The invoices show
the DRIVER's name and the DRIVER's GST registration number.

I've noticed that when you exclude the tip that the customer gave you, the gross amount
of the invoice matches the amount the driver was paid for the trip by Uber.

I've also noticed that Uber does not generate a trip invoice for all your trips, only
some of them.

Looking at a trip invoice, I think it's reasonable to assume that HST was charged to
the restaurant at 13% (in Ontario) by Uber, presumably paid to Uber by the restaurant,
and subsequently given to the driver by Uber.  In my opinion, that HST must now be
remitted to the CRA by the DRIVER.

I absolutely trust the advice of Intuit; I believe they describe how things should be
with food delivery drivers and HST but it seems to me that Uber is not following these
rules.

Since I have nothing further to go on, it's unclear to me what you should do,
especially since even HST that is collected in error must be remitted to the CRA.



What the script does
====================

Each trip invoice is a PDF file.  Uber Eats does not add up the amounts shown in
all these trip invoices.  If you have read the above background information and you
choose to remit the HST collected on your trip invoices, this script can help by
extracting the relevant fields directly from the PDF files themselves and adding up
the dollar amounts that you will need when filing your GST/HST statement.

The tax year is embedded into each trip invoice's file name.  For example, the trip invoice
named invoice-XXXXXXXX-03-2021-0000245.pdf was the 245th invoice for tax year 2021.

Unfortunately, you have to download each trip invoice pdf file manually and store them
somewhere on your Linux box.  If you do it daily or weekly it's not so bad.

The following reports are created and saved on disk for you.  The third report is also
displayed on your screen and should help you with lines 101 and 103 of your GST/HST
return.

   report1:  the raw text contained in all the PDF files for the given tax year with
             each invoice separated from the next with a row of equal signs
   report2:  selected fields from the same PDF files (in CSV format)
   report3:  grand totals of all the trip invoices (and of just those with HST applied)

Report3 has separate totals (with/without HST) in case you registered for GST/HST in that
tax year and you need to distinguish between the two time periods, before and after
registering.

Report1 is created by the script directly.  The script invokes the C programs to
generate report2 and report3.



-------------------
Larry Anta
larryanta@gmail.com
