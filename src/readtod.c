
#ifndef MAKEFILE_HAND
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "readtod.h"


#ifdef ACTPOL_DIRFILE
//#include "actpol/dirfile.h"
//#include "actpol/getdata.h"

void *ACTpolDirfile_read_channel( char typechar, const ACTpolDirfile *dirfile,
			    const char *channelname, int *nsamples_out );


typedef struct {
  char **fnames;
  ACTpolDirfile **formats;
  //void **formats;
  int nfiles;
} ManyFormatType;


#else
//#include "dirfile.h"
//#include "getdata.h"


typedef struct {
  char **fnames;
  struct FormatType **formats;
  //void **formats;
  int nfiles;
} ManyFormatType;


#endif


typedef struct {
  int nrow;               ///< Number of detector rows.
  int ncol;               ///< Number of detector columns.
  int nparam;             ///< Number of parameters in the pointing solution model.
  int fitType;            ///< Specify which model you're using for the pointing solution.
  actData **offsetAlt;      ///< Array of angular distance offsets in Alt direction (radians).
  actData **offsetAzCosAlt; ///< Array of angular distance offsets in the Az direction (radians).
  actData *fit;             ///< Best fit parameters and errors for fitType fit.
} mbPointingOffset2;



#define ACT_ARRAY_MAX_ROWS 33
#define ACT_ARRAY_MAX_COLS 32
#define ACT_ARRAY_MAX_DETECTORS (ACT_ARRAY_MAX_ROWS*ACT_ARRAY_MAX_COLS)

#define MAX(A,B) ((A > B) ? (A) : (B))


#ifdef ACTPOL_DIRFILE

void *dirfile_read_channel(char typechar, const ACTpolDirfile *dirfile,const char *channelname, int *nsamples_out )
{
  return ACTpolDirfile_read_channel(typechar,dirfile,channelname,nsamples_out);
}


float *dirfile_read_float_channel(const ACTpolDirfile *dirfile, const char *channelname, int *nsamples)
{
  return ACTpolDirfile_read_float_channel(dirfile,channelname,nsamples);
}

double *dirfile_read_double_channel(const ACTpolDirfile *dirfile, const char *channelname, int *nsamples)
{
  return ACTpolDirfile_read_double_channel(dirfile,channelname,nsamples);
}

uint32_t *dirfile_read_uint32_channel(const ACTpolDirfile *dirfile, const char *channelname, int *nsamples)
{
  return ACTpolDirfile_read_uint32_channel(dirfile,channelname,nsamples);
}



uint32_t dirfile_read_uint32_sample(const ACTpolDirfile *dirfile, const char *channelname, int index)
{
  return ACTpolDirfile_read_uint32_sample(dirfile,channelname,index);
}

bool dirfile_has_channel(const ACTpolDirfile *dirfile, const char *channel)
{
  return ACTpolDirfile_has_channel(dirfile,channel);
}



#endif


/*--------------------------------------------------------------------------------*/

void *dirfile_read_channel_direct(char typechar, const char *filename, const char *channelname, int *nsamples_out)
{
  int status;
  *nsamples_out=0;
#ifdef ACTPOL_DIRFILE
  ACTpolDirfile *format = ACTpolDirfile_open(filename);
#else
  struct FormatType *format = GetFormat( filename, NULL, &status );
#endif

  if (format==NULL) {
    fprintf(stderr,"Warning - problem reading format from file .%s.\n",filename);
    return NULL;
  }
  void *data=dirfile_read_channel(typechar,format,channelname,nsamples_out);
  if (data==NULL) {
    fprintf(stderr,"Error reading channel %s from %s\n",channelname,filename);

  }
#ifdef ACTPOL_DIRFILE
  ACTpolDirfile_close(format);
#else
  
  GetDataClose(format);
  free(format);
#endif
  
  return data;

}

// ----------------------------------------------------------------------------

void
write_tes_channelname( char *s, int row, int col )
{
    snprintf( s, 14, "tesdatar%2.2dc%2.2d", row, col );
}

// ----------------------------------------------------------------------------






mbTOD *
read_dirfile_tod_header( const char *filename )
{
  //printf("reading .%s.\n",filename);
    assert( filename != NULL );


    int status, n;
    //printf("reading format from %s\n",filename);

#ifdef ACTPOL_DIRFILE
    ACTpolDirfile *format = ACTpolDirfile_open(filename);
#else
    struct FormatType *format = GetFormat( filename, NULL, &status );
#endif

    assert( format != NULL );
    mbTOD *tod = (mbTOD *) calloc( 1,sizeof(mbTOD) );
    tod->dirfile=strdup(filename);
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // alt/az


#ifdef ACTDATA_DOUBLE
    float *az = dirfile_read_float_channel( format, "Enc_Az_Deg", &n );
    tod->ndata = n;
    tod->az=(actData *)malloc(sizeof(actData)*n);
    for (int i=0;i<tod->ndata;i++)
      tod->az[i]=az[i];
    free(az);
    float *alt = dirfile_read_float_channel( format, "Enc_El_Deg", &n );
    tod->alt=(actData *)malloc(sizeof(actData)*n);
    for (int i=0;i<n;i++)
      tod->alt[i]=alt[i];
    free(alt);

#else
    tod->az = dirfile_read_float_channel( format, "Enc_Az_Deg", &n );
    tod->ndata = n;
    tod->alt = dirfile_read_float_channel( format, "Enc_El_Deg", &n );
#endif
    assert( n == tod->ndata );

    //printf("in dirfile, az/alt[0] are %12.4g %12.4g with %d elements.\n",tod->az[0],tod->alt[0],n);

    // convert deg->rad; also kuka az -> astro az
    const float degToRad = M_PI/180;
    for ( int i = 0; i < tod->ndata; i++ )
    {
        tod->az[i] = (tod->az[i] + 180.) * degToRad;
        tod->alt[i] *= degToRad;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // ctime

    uint32_t s0, us0, s1, us1;
    s0 = dirfile_read_uint32_sample( format, "cpu_s", 0 );
    s1 = dirfile_read_uint32_sample( format, "cpu_s", tod->ndata-1 );
    us0 = dirfile_read_uint32_sample( format, "cpu_us", 0 );
    us1 = dirfile_read_uint32_sample( format, "cpu_us", tod->ndata-1 );
    //printf("s0,s1, us0, us1 are %8d %8d %8d %8d\n",s0,s1,us0,us1);
    

    double ctime0 = s0 + 1e-6*us0;
    double ctime1 = s1 + 1e-6*us1;

    tod->ctime = ctime0;
    tod->deltat = (ctime1 - ctime0)/(tod->ndata-1);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // row/col info

    tod->ndet = 0;
    tod->nrow = 0;
    tod->ncol = 0;
    tod->rows = malloc( ACT_ARRAY_MAX_DETECTORS*sizeof(int) );
    tod->cols = malloc( ACT_ARRAY_MAX_DETECTORS*sizeof(int) );
    char tesfield[] = "tesdatar00c00";

    for ( int r = 0; r < ACT_ARRAY_MAX_ROWS; r++ )
    for ( int c = 0; c < ACT_ARRAY_MAX_COLS; c++ )
    {
        write_tes_channelname( tesfield, r, c );
        if ( dirfile_has_channel(format, tesfield) )
        {
            int idet = tod->ndet++;
            tod->rows[idet] = r;
            tod->cols[idet] = c;
            tod->nrow = MAX( tod->nrow, r+1 );
            tod->ncol = MAX( tod->ncol, c+1 );
        }
    }
    tod->rows = (int *) realloc( tod->rows, tod->ndet*sizeof(int) );
    tod->cols = (int *) realloc( tod->cols, tod->ndet*sizeof(int) );

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // data
    tod->data=NULL;  //should already be set thusly.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


#ifdef ACTPOL_DIRFILE
  ACTpolDirfile_close(format);
#else

    GetDataClose(format);
    free(format);
#endif
    
    return tod;
}

// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------


mbTOD *
read_dirfile_tod_header_abs( const char *filename )
{
  //printf("reading .%s.\n",filename);
    assert( filename != NULL );


    int status, n,nn;
    //printf("reading format from %s\n",filename);
#ifdef ACTPOL_DIRFILE
    ACTpolDirfile *format = ACTpolDirfile_open(filename);
#else
    struct FormatType *format = GetFormat( filename, NULL, &status );
#endif   
    assert( format != NULL );
    mbTOD *tod = (mbTOD *) calloc( 1,sizeof(mbTOD) );
    tod->dirfile=strdup(filename);
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // alt/az


    {
      //do a bit of gyrating because these channels may have different lengths in the test data.
      float *tmp=dirfile_read_float_channel( format, "tesdatar00c00", &n );
      uint *az=dirfile_read_uint32_channel(format,"az_encoder_counts",&nn);
      if (nn<n)
	n=nn;
      tod->ndata=n;
      free(tmp);
      free(az);
      printf("n is %d\n",n);
    }

#ifdef ACTDATA_DOUBLE
    #if 0
    uint *az=dirfile_read_uint32_channel(format,"az_encoder_counts",&n);
    uint *alt=dirfile_read_uint32_channel(format,"el_encoder_counts",&n);
    tod->az=(actData *)malloc(sizeof(actData)*tod->ndata);
    tod->alt=(actData *)malloc(sizeof(actData)*tod->ndata);
    for (int i=0;i<tod->ndata;i++) {
      tod->az[i]=az[i];
      tod->alt[i]=alt[i];
    }
    printf("n is now %d\n",n);
    #else
    float *az = dirfile_read_float_channel( format, "AZ_DEGREES", &n );
    //tod->ndata = n;
    tod->az=(actData *)malloc(sizeof(actData)*tod->ndata);
    for (int i=0;i<tod->ndata;i++)
      tod->az[i]=az[i];
    free(az);
    float *alt = dirfile_read_float_channel( format, "EL_DEGREES", &n );
    tod->alt=(actData *)malloc(sizeof(actData)*n);
    for (int i=0;i<tod->ndata;i++)
      tod->alt[i]=alt[i];
    free(alt);
    #endif
#else
    tod->az = dirfile_read_float_channel( format, "AZ_DEGREES", &n );
    //tod->ndata = n;
    tod->alt = dirfile_read_float_channel( format, "EL_DEGREES", &n );
#endif
    assert( n == tod->ndata );
    
    //printf("in dirfile, az/alt[0] are %12.4g %12.4g with %d elements.\n",tod->az[0],tod->alt[0],n);
    
    // convert deg->rad; also kuka az -> astro az
    const float degToRad = M_PI/180;
    for ( int i = 0; i < tod->ndata; i++ )
      {
        tod->az[i] = (tod->az[i] + 180.) * degToRad;
        tod->alt[i] *= degToRad;
      }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // ctime

#if 0
    uint32_t s0, us0, s1, us1;
    s0 = dirfile_read_uint32_sample( format, "cpu_s", 0 );
    s1 = dirfile_read_uint32_sample( format, "cpu_s", tod->ndata-1 );
    us0 = dirfile_read_uint32_sample( format, "cpu_us", 0 );
    us1 = dirfile_read_uint32_sample( format, "cpu_us", tod->ndata-1 );
    //printf("s0,s1, us0, us1 are %8d %8d %8d %8d\n",s0,s1,us0,us1);
    


    double ctime0 = s0 + 1e-6*us0;
    double ctime1 = s1 + 1e-6*us1;

    tod->ctime = ctime0;
    tod->deltat = (ctime1 - ctime0)/(tod->ndata-1);
#else
    {
      double *tmp=dirfile_read_double_channel( format, "sync_time", &n );
      tod->ctime=tmp[0];
      tod->deltat=(tmp[n-1]-tmp[0])/((double)(n-1));
      free(tmp);
    }
#endif
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // row/col info
    
    tod->ndet = 0;
    tod->nrow = 0;
    tod->ncol = 0;
    tod->rows = malloc( ACT_ARRAY_MAX_DETECTORS*sizeof(int) );
    tod->cols = malloc( ACT_ARRAY_MAX_DETECTORS*sizeof(int) );
    char tesfield[] = "tesdatar00c00";

    for ( int r = 0; r < ACT_ARRAY_MAX_ROWS; r++ )
    for ( int c = 0; c < ACT_ARRAY_MAX_COLS; c++ )
    {
      write_tes_channelname( tesfield, r, c );
      if ( dirfile_has_channel(format, tesfield) )
        {
	  int idet = tod->ndet++;
	  tod->rows[idet] = r;
	  tod->cols[idet] = c;
	  tod->nrow = MAX( tod->nrow, r+1 );
	  tod->ncol = MAX( tod->ncol, c+1 );
        }
    }
    tod->rows = (int *) realloc( tod->rows, tod->ndet*sizeof(int) );
    tod->cols = (int *) realloc( tod->cols, tod->ndet*sizeof(int) );

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // data
    tod->data=NULL;  //should already be set thusly.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#ifdef ACTPOL_DIRFILE
  ACTpolDirfile_close(format);
#else
  
  GetDataClose(format);
  free(format);
#endif
    
    return tod;
}

// ----------------------------------------------------------------------------

actData *decimate_vector(actData *vec, int *nn) //shrink a vector by a factor of 2, free the old vector, return the new.
{
  int n=*nn;
  int n2=(n+1)/2;  //round up if we are odd
  
  actData *vec2=(actData *)malloc(n2*sizeof(actData));
  for (int i=0;i<n2-1;i++) {
    int ii=2*i;
    vec2[i]=0.25*(vec[ii]+2*vec[ii+1]+vec[ii+2]);
  }
  //Now do ends
  if (2*n2==n)  //if we have an even number of elements
    vec2[n2-1]=0.25*(vec[n-3]+2*vec[n-2]+vec[n-1]);
  else
    vec2[n2-1]=0.5*(vec[n-2]+vec[n-1]);
  
  free(vec);
  *nn=n2;
  return vec2;
  
  
  
}

// ----------------------------------------------------------------------------

void decimate_vector_in_place(actData *vec, int *nn) //shrink a vector by a factor of 2 in-place, replace nn by its new length.
{

  int n=*nn;
  int n2=(n+1)/2;  //round up if we are odd

  actData tmp0=0.25*(vec[0]+2*vec[1]*vec[2]);
  actData tmp1=0.25*(vec[2]+2*vec[3]*vec[4]);
  for (int i=2;i<n2-1;i++) {
    int ii=2*i;
    vec[i]=0.25*(vec[ii]+2*vec[ii+1]+vec[ii+2]);
  }
  if (2*n2==n)
    vec[n2-1]=0.25*(vec[n-3]+2*vec[n-2]+vec[n-1]);
  else  
    vec[n2-1]=0.5*(vec[n-2]+vec[n-1]);
  vec[0]=tmp0;
  vec[1]=tmp1;
  *nn=n2;

}



// ----------------------------------------------------------------------------


mbTOD *
read_dirfile_tod_header_decimate( const char *filename , int decimate )
{
  //fprintf(stderr,"inside read_dirfile_tod_header_decimate.\n");
  mbTOD *tod=read_dirfile_tod_header(filename);
  if (decimate==0)
    return tod;
  
  int n=tod->ndata;
  for (int i=0;i<decimate;i++) {
    int n1=n;
    tod->az=decimate_vector(tod->az,&n1);
    int n2=n;
    tod->alt=decimate_vector(tod->alt,&n2);
    assert(n2==n1);
    tod->deltat*=2.0;
    n=n1;
  }
  tod->ndata=n;
  tod->decimate=decimate;

  return tod;
    
}


// ----------------------------------------------------------------------------

actData **read_dirfile_tod_data_from_rowcol_list_old (mbTOD *tod, int *row, int *col, int ndet, actData **data, int *nout)
//if data is non-null, assume the space is allocated.
{
  
  int status;
  assert(tod!=NULL);
  //printf("reading format from %s.\n",tod->dirfile);

#ifdef ACTPOL_DIRFILE
  ACTpolDirfile *format = ACTpolDirfile_open(tod->dirfile);
#else
  struct FormatType *format = GetFormat( tod->dirfile, NULL, &status );
#endif
  //printf("got it.\n");
  assert( format != NULL );
  
  
  
  if (data==NULL) {
    actData *vec=(actData *)malloc(ndet*tod->ndata*sizeof(actData));
    assert(vec!=NULL);
    for (int i=0;i<ndet;i++)
      data[i]=vec+i*tod->ndata;
  }
  
  for (int idet=0;idet<ndet;idet++) {
    assert(row[idet]<33);
    assert(col[idet]<32);
    char tesfield[]="tesdatar00c00";
    sprintf(tesfield,"tesdatar%02dc%02d",row[idet],col[idet]);
    int n;
#ifndef ACTDATA_DOUBLE
    //printf("reading float channel\n");
    actData *chan = dirfile_read_float_channel( format, tesfield, &n );
#else
    //printf("reading double channel\n");
    actData *chan = dirfile_read_double_channel( format, tesfield, &n );
#endif
    //printf("finished channel.\n");
    for (int ii=0;ii<tod->decimate;ii++)
      chan=decimate_vector(chan,&n);
    if (n>tod->ndata) {
#pragma omp critical 
      {
	static int firsttime=1;
	if (firsttime) {
	  firsttime=0;
	  fprintf(stderr,"Warning - data is longer than the TOD.  You should be very nervous about this, unless you expected to see this message.\n");
	}
      }
    }
    else {
      assert(n==tod->ndata);
      *nout=n;
    }
    memcpy(data[idet],chan+tod->start_offset,sizeof(actData)*tod->ndata);
    free(chan);
  }
  return data;
}


// ----------------------------------------------------------------------------
ManyFormatType *GetManyFormat(char *fname, void *foo, int *status)
{
  printf("filename is x%sx\n",fname);
  int nn=strlen(fname);
  printf("file has %d characters.\n",nn);
  int nfiles=1;
  for (int i=0;i<nn;i++)
    if (fname[i]==10)
      nfiles++;
  printf("have %d files.\n",nfiles);
  char **fnames=(char **)malloc(sizeof(char *)*nfiles);
  char *mystr=strdup(fname);
  fnames[0]=mystr;
  int ii=1;
  for (int i=0;i<nn;i++) {
    if (mystr[i]==10) {
      mystr[i]=0;
      fnames[ii]=mystr+i+1;
      ii++;
    }
  }
  for (int i=0;i<nfiles;i++)
    printf("file %d is %s\n",i,fnames[i]);
  printf("have %d bytes.\n",sizeof( ManyFormatType));
  ManyFormatType *mft=(ManyFormatType *)malloc(sizeof( ManyFormatType));
  mft->nfiles=nfiles;
  mft->fnames=fnames;
#ifdef ACTPOL_DIRFILE
  mft->formats=(ACTpolDirfile **)malloc(sizeof(ACTpolDirfile *)*mft->nfiles);
#else
  mft->formats=(struct FormatType **)malloc(sizeof(struct FormatType *)*mft->nfiles);
#endif
  for (int i=0;i<mft->nfiles;i++) {
#ifdef ACTPOL_DIRFILE
    mft->formats[i]=ACTpolDirfile_open(mft->fnames[i]);
#else
    mft->formats[i]=GetFormat(mft->fnames[i],NULL,status);
#endif
    if (*status)
      printf("file %s had a problem reading format.\n",mft->fnames[i]);
  }
  printf("read my formats.\n");
  return mft;
}
/*--------------------------------------------------------------------------------*/
void FreeManyFormat(ManyFormatType *fmt)
{
  for (int i=0;i<fmt->nfiles;i++) {
#ifdef ACTPOL_DIRFILE
    ACTpolDirfile_close(fmt->formats[i]);
#else
    GetDataClose(fmt->formats[i]);
    free(fmt->formats[i]);    
#endif
  }
  free(fmt);
}
// ----------------------------------------------------------------------------
#ifdef ACTPOL_DIRFILE
actData *dirfile_read_actdata_channel(const ACTpolDirfile *fmt, char *channame, int *n)
#else
actData *dirfile_read_actdata_channel(struct FormatType *fmt, char *channame, int *n)
#endif
{
#ifndef ACTDATA_DOUBLE
  //printf("reading float channel\n");
  return dirfile_read_float_channel( fmt, channame, n );
#else
  //printf("reading double channel\n");
  return dirfile_read_double_channel( fmt, channame, n );
#endif

}
// ----------------------------------------------------------------------------
actData *many_dirfile_read_actData_channel(ManyFormatType *fmt, char *channame, int *n)
{
  if (fmt->nfiles==1)
    return dirfile_read_actdata_channel(fmt->formats[0],channame,n);

  int *ndata=(int *)malloc(sizeof(int)*fmt->nfiles);
  actData **vecs=(actData **)malloc(sizeof(actData *)*fmt->nfiles);
  for (int i=0;i<fmt->nfiles;i++) {
    vecs[i]=dirfile_read_actdata_channel(fmt->formats[i],channame,&ndata[i]);
    //printf("ndata on %d is %d\n",i,ndata[i]);
  }
  int ntot=0;
  for (int i=0;i<fmt->nfiles;i++)
    ntot+=ndata[i];
  actData *vec=(actData *)malloc(ntot*sizeof(actData));
  int istart=0;
  for (int i=0;i<fmt->nfiles;i++) {
    memcpy(vec+istart,vecs[i],ndata[i]*sizeof(actData));
    istart+=ndata[i];
  }
  
  free(ndata);
  for (int i=0;i<fmt->nfiles;i++)
    free(vecs[i]);
  free(vecs);
  *n=ntot;
  return vec;
}

// ----------------------------------------------------------------------------

actData **read_dirfile_tod_data_from_rowcol_list (mbTOD *tod, int *row, int *col, int ndet, actData **data, int *nout)
//if data is non-null, assume the space is allocated.
{
  
  int status;
  assert(tod!=NULL);
  //printf("reading format from %s.\n",tod->dirfile);
  ManyFormatType *format=GetManyFormat(tod->dirfile,NULL,&status);
  

  //struct FormatType *format = GetFormat( tod->dirfile, NULL, &status );
  //printf("got it.\n");
  assert( format != NULL );
  

  
  if (data==NULL) {
    actData *vec=(actData *)malloc(ndet*tod->ndata*sizeof(actData));
    assert(vec!=NULL);
    for (int i=0;i<ndet;i++)
      data[i]=vec+i*tod->ndata;
  }
  
  for (int idet=0;idet<ndet;idet++) {
    assert(row[idet]<33);
    assert(col[idet]<32);
    char tesfield[]="tesdatar00c00";
    sprintf(tesfield,"tesdatar%02dc%02d",row[idet],col[idet]);
    int n;

    actData *chan=many_dirfile_read_actData_channel(format,tesfield,&n);

#ifndef ACTDATA_DOUBLE
    //printf("reading float channel\n");
    //actData *chan = dirfile_read_float_channel( format, tesfield, &n );
#else
    //printf("reading double channel\n");
    //actData *chan = dirfile_read_double_channel( format, tesfield, &n );
#endif
    //printf("finished channel.\n");
    for (int ii=0;ii<tod->decimate;ii++)
      chan=decimate_vector(chan,&n);
    if (n>tod->ndata) {
#pragma omp critical 
      {
	static int firsttime=1;
	if (firsttime) {
	  firsttime=0;
	  fprintf(stderr,"Warning - data is longer than the TOD.  You should be very nervous about this, unless you expected to see this message.\n");
	}
      }
    }
    else {
      assert(n==tod->ndata);
      *nout=n;
    }
    memcpy(data[idet],chan+tod->start_offset,sizeof(actData)*tod->ndata);
    free(chan);
  }

  FreeManyFormat(format);

  return data;
}



// ----------------------------------------------------------------------------

void read_dirfile_tod_data (mbTOD *tod)
{
#if 1
  assert(tod!=NULL);
  if (!tod->have_data)
    tod->data=NULL;
  int nout=0;
  tod->data=read_dirfile_tod_data_from_rowcol_list(tod,tod->rows,tod->cols,tod->ndet,tod->data,&nout);
  tod->have_data=1;
  

  
#else
  int status;
  assert(tod!=NULL);
  struct FormatType *format = GetFormat( tod->dirfile, NULL, &status );
  assert( format != NULL );

  if (tod->data==NULL) {
    assert(tod->ndata>0);  //make sure we know how big we ought to be.
    tod->data = (actData **) malloc( tod->ndet*sizeof(actData *) );
    actData *vec=(actData *)malloc(tod->ndet*tod->ndata*sizeof(actData));
    for (int i=0;i<tod->ndet;i++)
      tod->data[i]=&(vec[i*tod->ndata]);
  }
  //try skipping parallel read in case it bogs disks down.
  //#pragma omp parallel for shared(tod,format) default(none)    
  for ( int idet = 0; idet < tod->ndet; idet++ )  {
    char tesfield[] = "tesdatar00c00";
    int n;
    int row = tod->rows[idet];
    int col = tod->cols[idet];
    write_tes_channelname( tesfield, row, col );
#ifndef ACTDATA_DOUBLE
    actData *chan = dirfile_read_float_channel( format, tesfield, &n );
    for (int i=0;i<tod->decimate;i++)
      chan=decimate_vector(chan,&n);
    assert( n == tod->ndata );
    memcpy(tod->data[idet],chan,sizeof(actData)*tod->ndata);
    
#else
    double *chan =  dirfile_read_double_channel( format, tesfield, &n );
    for (int i=0;i<tod->decimate;i++)
      chan=decimate_vector(chan,&n);
    
    if (n!=tod->ndata) {
      printf("Warning - size mismatch in read_tod_data, %d vs. %d on detector %d %d, and file %s\n",n,tod->ndata,tod->rows[idet],tod->cols[idet],tod->dirfile);
    }
    assert( n == tod->ndata );
    for (int i=0;i<tod->ndata;i++)
      tod->data[idet][i]=chan[i];
    //memcpy(tod->data[idet],chan,sizeof(actData)*tod->ndata);

#endif
    free(chan);
  }
#endif
}

// ----------------------------------------------------------------------------


mbTOD *
read_dirfile_tod( const char *filename )
{
  mbTOD *tod=read_dirfile_tod_header(filename );
  read_dirfile_tod_data(tod);
  return tod;
}


