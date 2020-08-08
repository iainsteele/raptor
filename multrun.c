
/* 
gcc -DC_GNU64=400 -DOS_LINUX -I /usr/local/xclib/inc multrun.c /usr/local/xclib/lib/xclib_x86_64.a -lm -l cfitsio -o multrun
*/

#define FORMATFILE	"/home/eng/src/rap_1000ms.fmt"	  // using format file saved by XCAP

#if !defined(UNITS)
    #define UNITS	1
#endif
#define UNITSMAP    ((1<<UNITS)-1)  // shorthand - bitmap of all units

 #define DRIVERPARMS ""	      // default
 
 #if !defined(USE_PXIPL)
    #define USE_PXIPL	0
#endif

#if !defined(IMAGEFILE_DIR)
    #define IMAGEFILE_DIR    "/home/eng/rapdata"
#endif

#include <stdio.h>		// c library
#include <signal.h>		// c library
#include <string.h>		// c library
#include <stdlib.h>		// c library
#include <stdarg.h>		// c library
#include <unistd.h>		// c library
#include <limits.h>		// c library
#include <sys/time.h>		// c library
#include "xcliball.h"		// function prototypes

#if USE_PXIPL
  #include "pxipl.h"		// function prototypes
#endif

#include <fitsio.h>

#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <time.h>

void panic(char *panic_string);
void logit(char *log_string);

void make_filename(char *filename);

int file_select(const struct direct *entry);

char comparison_string[80];
char datadir[80];
char prefix[80];
char exptype ='e';



int file_select (const struct direct *entry) {
  
  if ((strncmp(comparison_string, entry->d_name, strlen(comparison_string))==0) 
      &&
      (strstr(entry->d_name,"_1_1_0.fits")!=0))
    return (TRUE);

  return (FALSE);
}

void logit(char *log_string) {
  fprintf(stderr,"%s\n",log_string);
}
 
void panic(char *panic_string) {
  printf("multrun: %s\n",panic_string);
  exit(1);
}

void make_filename(char *filename) {

  time_t nowbin;

  char nowstring[80];
  const struct tm *nowstruct;

  struct direct **files;

  int day;
  int month;
  int year;
  int hour;

  int numfiles;

  //  (void)setlocale(LC_ALL,"");

  if (time(&nowbin) == (time_t)-1)
    panic("could not get time of day");
  
  nowstruct=localtime(&nowbin);

  strftime(nowstring, 80, "%d", nowstruct);
  day = atoi(nowstring);

  strftime(nowstring, 80, "%m", nowstruct);
  month = atoi(nowstring);

  strftime(nowstring, 80, "%Y", nowstruct);
  year = atoi(nowstring);

  strftime(nowstring, 80, "%H", nowstruct);
  hour = atoi(nowstring);


  if (year>2032) panic("year>2032!");


  printf("%d %d %d %d\n", year, month, day, hour);

  if (hour<=11) day--;
  
  if (day==0) {
    month--;
    day=31;
    if (month==9) day=30;
    if (month==4) day=30;
    if (month==6) day=30;
    if (month==11) day=30;
    if (month==2) {
      day=28;
      if ((year==2008) ||
	  (year==2012) ||
	  (year==2016) ||
	  (year==2020) ||
 	  (year==2024) ||
 	  (year==2028) ||
 	  (year==2032)) 
	day=29;
    }
  }

  if (month==0) {
    year--;
    month=12;
    day=31;
  }


  sprintf(comparison_string,"%s_e_%d%02d%02d",prefix,year,month,day);
  numfiles=scandir(datadir, &files, file_select, alphasort);

  sprintf(comparison_string,"%s_b_%d%02d%02d",prefix,year,month,day);
  numfiles=numfiles+scandir(datadir, &files, file_select, alphasort);

  sprintf(comparison_string,"%s_d_%d%02d%02d",prefix,year,month,day);
  numfiles=numfiles+scandir(datadir, &files, file_select, alphasort);

  sprintf(comparison_string,"%s_f_%d%02d%02d",prefix,year,month,day);
  numfiles=numfiles+scandir(datadir, &files, file_select, alphasort);
  

  sprintf(filename,"%s/%s_%c_%d%02d%02d_%d_",
	  datadir,prefix,exptype,year,month,day,numfiles+1);
  
}



/*
 *  SUPPORT STUFF:
 *
 *  Catch CTRL+C and floating point exceptions so that
 *  once opened, the PIXCI(R) driver and frame grabber
 *  are always closed before exit.
 *  In most environments, this isn't critical; the operating system
 *  advises the PIXCI(R) driver that the program has terminated.
 *  In other environments, such as DOS, the operating system
 *  isn't as helpful and the driver & frame grabber would remain open.
 */
static void sigintfunc(int sig)
{
    /*
     * Some languages/environments don't allow
     * use of printf from a signal catcher.
     * Printing the message isn't important anyhow.
     *
    if (sig == SIGINT)
	printf("Break\n");
    if (sig == SIGFPE)
	printf("Float\n");
    */

    pxd_PIXCIclose();
    exit(1);
}


/*
 * Video 'interrupt' callback function.
 */
static int fieldirqcount = 0;
static void videoirqfunc(int sig)
{
    fieldirqcount++;
}

/*
 * Report image frame buffer memory size
 */
static void do_imsize(void)
{
    printf("Frame buffer memory size       : %.3f Kbytes\n", (double)pxd_infoMemsize(UNITSMAP)/1024);
    printf("Image frame buffers (per board): %d\n", pxd_imageZdim());
    printf("Number of boards               : %d\n", pxd_infoUnits());
}

int xdim, ydim;


/*
 * Report image resolution.
 */
static void do_vidsize(void)
{

    xdim=pxd_imageXdim();
    ydim=pxd_imageYdim();
    printf("Image resolution:\n");
    printf("xdim           = %d\n", xdim);
    printf("ydim           = %d\n", ydim);
    printf("colors         = %d\n", pxd_imageCdim());
    printf("bits per pixel = %d\n", pxd_imageCdim()*pxd_imageBdim());
}



static void copy_frame(ushort *monoimage_buf16,int buf_num)
{
    int     i, u;
    
    
    
    //
    // Transfer the monochrome data from a selected AOI of
    // the image buffer into the PC buffer, as 8 bit pixels.
    // Or,
    // Transfer the monochrome data from a selected AOI of
    // the image buffer into the PC buffer, as 8 to 16 bit pixels.
    //
    // The ushort array could be used for both for 8 bit pixels, but
    // users of 8 bit pixels commonly assume pixel<=>byte,
    // and is more efficient.
    //
    for (u = 0; u < UNITS; u++) {
	    i = pxd_readushort(1<<u, buf_num, 0, 0, xdim, ydim, 
	    monoimage_buf16, xdim*ydim, "Grey"); 
	    
		if (UNITS > 1)
		    printf("Unit %d: ", u);

		if (i < 0)
		    printf("pxd_readushort: %s\n", pxd_mesgErrorCode(i));
		
		    printf("pxd_readushort %d bytes of %d transfered\n", i, xdim*ydim);
	    }
}

int main(int argc, char* argv[])
{

    ushort *monoimage_buf16;
    fitsfile *fptr;
    int fits_status=0;
    long naxes[2];

    int coadds = 1; /* coadds are added in software (making a run) before writing FITS files */
    int runs = 1; /* runs (added up coadds) are written as separate FITS files */

    int exptime = 1000; /* 1000 ms default */

    char formatfile[80];

    switch(argc) {	
      case 2:
	coadds=atoi(argv[1]);
	break;

      case 3:
	coadds=atoi(argv[1]);
	exptime=atoi(argv[2]);
	break;

      case 4:
	coadds=atoi(argv[1]);
	exptime=atoi(argv[2]);
	runs=atoi(argv[3]);
	break;

       default:
	 panic("parameters are <coadds> <exptime-in-msec> <multrun>");
	 
      }
    
    printf("coadds = %d\n",coadds);
    printf("exptime = %d msec\n",exptime);
    printf("multruns = %d\n",runs);


    switch(exptime) {
    case 1000:
      strcpy(formatfile, "/home/eng/src/rap_1000ms.fmt");
      break;    
    case 100:
      strcpy(formatfile, "/home/eng/src/rap_100ms.fmt");
      break;    
    default:
      panic("unsupported exposure time!");
    }
      
    
    // Catch signals.
    signal(SIGINT, sigintfunc);
    signal(SIGFPE, sigintfunc);

    // Open and set video format.

    int o=pxd_PIXCIopen(DRIVERPARMS, "", formatfile);
    if (o < 0) {
	printf("Open Error %s(%d)\a\a\n", pxd_mesgErrorCode(o), o);
	pxd_mesgFault(UNITSMAP);
	return(o);
    }
    
    // Basic video operations
    // Report info,
    
    do_imsize();
    do_vidsize();
    
    monoimage_buf16=(ushort *)malloc(xdim*ydim*sizeof(ushort));
    int *coadd_array;

    coadd_array = (int *)malloc(xdim*ydim*sizeof(int));

    double *mean_array;
    mean_array = (double *)malloc(xdim*ydim*sizeof(double));
    
    //setup FITS filename

    char filename[1000];
    char fileprefix[1000];
    make_filename(fileprefix);

   /* Create fileprefix */
    strcpy(datadir,"/home/eng/src/data");
    strcpy(prefix,"x");
    make_filename(fileprefix);

    printf("fileprefix = %s\n",fileprefix);
    
    int run=1;

    pxbuffer_t lastbuf=0;
    
    
    for (run=1; run<=runs; run++) {
    
    sprintf(filename,"%s%d_1_0.fits",fileprefix,run);
	    
    /* Create the output file */
    fits_create_file(&fptr, filename, &fits_status);

    printf("filename = %s\n",filename);

    naxes[0]=xdim;
    naxes[1]=ydim;   
    fits_create_img(fptr,-64, 2, naxes, &fits_status);
    

    time_t nowbin;
    char nowstring[80];
    char datestring[80];
    char utstring[80];
    const struct tm *nowstruct;

    int i;
    
    nowstruct=localtime(&nowbin);
    
    strftime(nowstring, 80, "%Y-%m-%dT%H:%M:%S", nowstruct);
    strftime(datestring, 80, "%Y-%m-%d", nowstruct);
    strftime(utstring, 80, "%H:%M:%S", nowstruct);
    
    fits_write_key(fptr, TSTRING, "ORIGIN", "Liverpool JMU",
		   "Liverpool Telescope Project", &fits_status);

    fits_write_key(fptr, TSTRING, "INSTRUME", "Raptor",
		   "Instrument used.", &fits_status);

    fits_write_key(fptr, TSTRING, "DATE", datestring,
		   "[UTC] Date of obs.", &fits_status);
  
    fits_write_key(fptr, TSTRING, "DATE-OBS", nowstring,
		   "[UTC] APPROX start of obs.", &fits_status);
  
    fits_write_key(fptr, TSTRING, "UTSTART", utstring,
		   "[UTC] APPROX start time of obs.", &fits_status);
  
    
    double exposure_in_seconds = coadds*exptime/1000.0;
    
    fits_write_key(fptr, TDOUBLE, "EXPTIME", &exposure_in_seconds, 
    		   "[seconds] Total Exposure length.", &fits_status);

    double coadd_time = exptime/1000.0;
    
    fits_write_key(fptr, TDOUBLE, "COADDSEC", &coadd_time, 
    		   "[seconds] Length of an individual coadd", &fits_status);

    fits_write_key(fptr, TINT, "COADDNUM", &coadds, 
    		   "[seconds] Number of coadds", &fits_status);


    for (i=0; i<xdim*ydim; i++) {
	  coadd_array[i]=0;
    }

    int coadd;

    lastbuf = 0;
    
    //do alternate capture into frame buffers 1 and 2
    pxd_goLivePair(1,1,2);
    
    for (coadd=1;coadd<=coadds;coadd++) {
      printf("starting coadd %d with lastbuf %d\n",coadd,(int)lastbuf);
      // wait for new buffer
      while (pxd_capturedBuffer(1)==lastbuf) {
      usleep(500);  // I *guess* this sleep is interruputed by SIGINT. Processor load lower with it in and fewer dropped frames!
      }
      lastbuf = pxd_capturedBuffer(1);
      copy_frame(monoimage_buf16,lastbuf);
      printf("captured coadd %d of %d from buffer %d\n",coadd,coadds,(int)lastbuf);

      for (i=0; i<xdim*ydim; i++) {
	  coadd_array[i]=coadd_array[i]+monoimage_buf16[i];
      }
	
    }      
	
    pxd_goAbortLive(1);

    for (i=0; i<xdim*ydim; i++) {
      mean_array[i] = (float)coadd_array[i]/coadds;
    }
    
    fits_write_img(fptr, TDOUBLE, 1, xdim*ydim, mean_array, &fits_status);
    
    /* close file */
    fits_close_file(fptr, &fits_status);

    

    /* if error occured, print out error message */
    if (fits_status) fits_report_error(stderr, fits_status);
  
}
    
    //
    // All done
    //
    pxd_PIXCIclose();
    logit("PIXCI(R) frame grabber closed");

    return(0);
}
    
