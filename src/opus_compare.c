/* Copyright (c) 2011-2012 Xiph.Org Foundation, Mozilla Corporation
   Written by Jean-Marc Valin and Timothy B. Terriberry */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define OPUS_PI (3.14159265F)

#define OPUS_COSF(_x)        ((float)cos(_x))
#define OPUS_SINF(_x)        ((float)sin(_x))

static void *check_alloc(void *_ptr){
  if(_ptr==NULL){
    fprintf(stderr,"Out of memory.\n");
    exit(EXIT_FAILURE);
  }
  return _ptr;
}

static void *opus_malloc(size_t _size){
  return check_alloc(malloc(_size));
}

static void *opus_realloc(void *_ptr,size_t _size){
  return check_alloc(realloc(_ptr,_size));
}

static size_t read_pcm16(float **_samples,FILE *_fin,int _nchannels){
  unsigned char  buf[1024];
  float         *samples;
  size_t         nsamples;
  size_t         csamples;
  size_t         xi;
  size_t         nread;
  samples=NULL;
  nsamples=csamples=0;
  for(;;){
    nread=fread(buf,2*_nchannels,1024/(2*_nchannels),_fin);
    if(nread<=0)break;
    if(nsamples+nread>csamples){
      do csamples=csamples<<1|1;
      while(nsamples+nread>csamples);
      samples=(float *)opus_realloc(samples,
       _nchannels*csamples*sizeof(*samples));
    }
    for(xi=0;xi<nread;xi++){
      int ci;
      for(ci=0;ci<_nchannels;ci++){
        int s;
        s=buf[2*(xi*_nchannels+ci)+1]<<8|buf[2*(xi*_nchannels+ci)];
        s=((s&0xFFFF)^0x8000)-0x8000;
        samples[(nsamples+xi)*_nchannels+ci]=s;
      }
    }
    nsamples+=nread;
  }
  *_samples=(float *)opus_realloc(samples,
   _nchannels*nsamples*sizeof(*samples));
  return nsamples;
}

static void band_energy(float *_out,float *_ps,const int *_bands,int _nbands,
 const float *_in,int _nchannels,size_t _nframes,int _window_sz,
 int _step,int _downsample){
  float *window;
  float *x;
  float *c;
  float *s;
  size_t xi;
  int    xj;
  int    ps_sz;
  window=(float *)opus_malloc((3+_nchannels)*_window_sz*sizeof(*window));
  c=window+_window_sz;
  s=c+_window_sz;
  x=s+_window_sz;
  ps_sz=_window_sz/2;
  for(xj=0;xj<_window_sz;xj++){
    window[xj]=0.5F-0.5F*OPUS_COSF((2*OPUS_PI/(_window_sz-1))*xj);
  }
  for(xj=0;xj<_window_sz;xj++){
    c[xj]=OPUS_COSF((2*OPUS_PI/_window_sz)*xj);
  }
  for(xj=0;xj<_window_sz;xj++){
    s[xj]=OPUS_SINF((2*OPUS_PI/_window_sz)*xj);
  }
  for(xi=0;xi<_nframes;xi++){
    int ci;
    int xk;
    int bi;
    for(ci=0;ci<_nchannels;ci++){
      for(xk=0;xk<_window_sz;xk++){
        x[ci*_window_sz+xk]=window[xk]*_in[(xi*_step+xk)*_nchannels+ci];
      }
    }
    for(bi=xj=0;bi<_nbands;bi++){
      float p[2]={0};
      for(;xj<_bands[bi+1];xj++){
        for(ci=0;ci<_nchannels;ci++){
          float re;
          float im;
          int   ti;
          ti=0;
          re=im=0;
          for(xk=0;xk<_window_sz;xk++){
            re+=c[ti]*x[ci*_window_sz+xk];
            im-=s[ti]*x[ci*_window_sz+xk];
            ti+=xj;
            if(ti>=_window_sz)ti-=_window_sz;
          }
          re*=_downsample;
          im*=_downsample;
          _ps[(xi*ps_sz+xj)*_nchannels+ci]=re*re+im*im+100000;
          p[ci]+=_ps[(xi*ps_sz+xj)*_nchannels+ci];
        }
      }
      if(_out){
        _out[(xi*_nbands+bi)*_nchannels]=p[0]/(_bands[bi+1]-_bands[bi]);
        if(_nchannels==2){
          _out[(xi*_nbands+bi)*_nchannels+1]=p[1]/(_bands[bi+1]-_bands[bi]);
        }
      }
    }
  }
  free(window);
}

static const short eband5ms[] = {
/*0  200 400 600 800  1k 1.2 1.4 1.6  2k 2.4 2.8 3.2  4k 4.8 5.6 6.8  8k 9.6 12k 15.6 */
  0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 34, 40, 48, 60, 78, 100
};

/* Defining 25 critical bands for the full 0-20 kHz audio bandwidth
   Taken from http://ccrma.stanford.edu/~jos/bbt/Bark_Frequency_Scale.html */
#define BARK_BANDS 25
static const short bark_freq[BARK_BANDS+1] = {
      0,   100,   200,   300,   400,
    510,   630,   770,   920,  1080,
   1270,  1480,  1720,  2000,  2320,
   2700,  3150,  3700,  4400,  5300,
   6400,  7700,  9500, 12000, 15500,
  20000};

static short *compute_ebands(int Fs, int frame_size, int res, int *nbEBands)
{
   short *eBands;
   int i, j, lin, low, high, nBark, offset=0;

   /* All modes that have 2.5 ms short blocks use the same definition */
   if (Fs == 400*(int)frame_size)
   {
      *nbEBands = sizeof(eband5ms)/sizeof(eband5ms[0])-1;
      eBands = opus_malloc(sizeof(short)*(*nbEBands+1));
      for (i=0;i<*nbEBands+1;i++)
         eBands[i] = eband5ms[i];
      return eBands;
   }
   /* Find the number of critical bands supported by our sampling rate */
   for (nBark=1;nBark<BARK_BANDS;nBark++)
    if (bark_freq[nBark+1]*2 >= Fs)
       break;

   /* Find where the linear part ends (i.e. where the spacing is more than min_width */
   for (lin=0;lin<nBark;lin++)
      if (bark_freq[lin+1]-bark_freq[lin] >= res)
         break;

   low = (bark_freq[lin]+res/2)/res;
   high = nBark-lin;
   *nbEBands = low+high;
   eBands = opus_malloc(sizeof(short)*(*nbEBands+2));

   if (eBands==NULL)
      return NULL;

   /* Linear spacing (min_width) */
   for (i=0;i<low;i++)
      eBands[i] = i;
   if (low>0)
      offset = eBands[low-1]*res - bark_freq[lin-1];
   /* Spacing follows critical bands */
   for (i=0;i<high;i++)
   {
      int target = bark_freq[lin+i];
      /* Round to an even value */
      eBands[i+low] = (target+offset/2+res)/(2*res)*2;
      offset = eBands[i+low]*res - target;
   }
   /* Enforce the minimum spacing at the boundary */
   for (i=0;i<*nbEBands;i++)
      if (eBands[i] < i)
         eBands[i] = i;
   /* Round to an even value */
   eBands[*nbEBands] = (bark_freq[nBark]+res)/(2*res)*2;
   if (eBands[*nbEBands] > frame_size)
      eBands[*nbEBands] = frame_size;
   for (i=1;i<*nbEBands-1;i++)
   {
      if (eBands[i+1]-eBands[i] < eBands[i]-eBands[i-1])
      {
         eBands[i] -= (2*eBands[i]-eBands[i-1]-eBands[i+1])/2;
      }
   }
   /* Remove any empty bands. */
   for (i=j=0;i<*nbEBands;i++)
      if(eBands[i+1]>eBands[j])
         eBands[++j]=eBands[i+1];
   *nbEBands=j;

   return eBands;
}

#define NBANDS (21)
#define NFREQS (240)

/*Bands on which we compute the pseudo-NMR (Bark-derived
  CELT bands).*/
static const int BANDS[NBANDS+1]={
  0,2,4,6,8,10,12,14,16,20,24,28,32,40,48,56,68,80,96,120,156,200
};

#define TEST_WIN_SIZE (480)
#define TEST_WIN_STEP (120)

static int
verify(int nchannels, float *x, size_t xlength, float *y,
       unsigned rate)
{
  float   *xb;
  float   *X;
  float   *Y;
  size_t   xi;
  int      ci;
  int      xj;
  int      bi;
  int      max_compare;
  double    err;
  float    Q;
  size_t nframes;
  int      downsample;
  int      ybands;
  int      yfreqs;
  // TODO: do similar adjustments for X as well
  compute_ebands(rate, 480, rate / 480, &ybands);
  downsample=48000/rate;
  yfreqs=NFREQS/downsample;

  nframes=(xlength-TEST_WIN_SIZE+TEST_WIN_STEP)/TEST_WIN_STEP;
  xb=(float *)opus_malloc(nframes*NBANDS*nchannels*sizeof(*xb));
  X=(float *)opus_malloc(nframes*NFREQS*nchannels*sizeof(*X));
  Y=(float *)opus_malloc(nframes*yfreqs*nchannels*sizeof(*Y));
  /*Compute the per-band spectral energy of the original signal
     and the error.*/
  band_energy(xb,X,BANDS,NBANDS,x,nchannels,nframes,
   TEST_WIN_SIZE/downsample,TEST_WIN_STEP/downsample,downsample);
  free(x);
  band_energy(NULL,Y,BANDS,ybands,y,nchannels,nframes,
   TEST_WIN_SIZE/downsample,TEST_WIN_STEP/downsample,downsample);
  free(y);
  for(xi=0;xi<nframes;xi++){
    /*Frequency masking (low to high): 10 dB/Bark slope.*/
    for(bi=1;bi<NBANDS;bi++){
      for(ci=0;ci<nchannels;ci++){
        xb[(xi*NBANDS+bi)*nchannels+ci]+=
         0.1F*xb[(xi*NBANDS+bi-1)*nchannels+ci];
      }
    }
    /*Frequency masking (high to low): 15 dB/Bark slope.*/
    for(bi=NBANDS-1;bi-->0;){
      for(ci=0;ci<nchannels;ci++){
        xb[(xi*NBANDS+bi)*nchannels+ci]+=
         0.03F*xb[(xi*NBANDS+bi+1)*nchannels+ci];
      }
    }
    if(xi>0){
      /*Temporal masking: -3 dB/2.5ms slope.*/
      for(bi=0;bi<NBANDS;bi++){
        for(ci=0;ci<nchannels;ci++){
          xb[(xi*NBANDS+bi)*nchannels+ci]+=
           0.5F*xb[((xi-1)*NBANDS+bi)*nchannels+ci];
        }
      }
    }
    /* Allowing some cross-talk */
    if(nchannels==2){
      for(bi=0;bi<NBANDS;bi++){
        float l,r;
        l=xb[(xi*NBANDS+bi)*nchannels+0];
        r=xb[(xi*NBANDS+bi)*nchannels+1];
        xb[(xi*NBANDS+bi)*nchannels+0]+=0.01F*r;
        xb[(xi*NBANDS+bi)*nchannels+1]+=0.01F*l;
      }
    }

    /* Apply masking */
    for(bi=0;bi<ybands;bi++){
      for(xj=BANDS[bi];xj<BANDS[bi+1];xj++){
        for(ci=0;ci<nchannels;ci++){
          X[(xi*NFREQS+xj)*nchannels+ci]+=
           0.1F*xb[(xi*NBANDS+bi)*nchannels+ci];
          Y[(xi*yfreqs+xj)*nchannels+ci]+=
           0.1F*xb[(xi*NBANDS+bi)*nchannels+ci];
        }
      }
    }
  }

  /* Average of consecutive frames to make comparison slightly less sensitive */
  for(bi=0;bi<ybands;bi++){
    for(xj=BANDS[bi];xj<BANDS[bi+1];xj++){
      for(ci=0;ci<nchannels;ci++){
         float xtmp;
         float ytmp;
         xtmp = X[xj*nchannels+ci];
         ytmp = Y[xj*nchannels+ci];
         for(xi=1;xi<nframes;xi++){
           float xtmp2;
           float ytmp2;
           xtmp2 = X[(xi*NFREQS+xj)*nchannels+ci];
           ytmp2 = Y[(xi*yfreqs+xj)*nchannels+ci];
           X[(xi*NFREQS+xj)*nchannels+ci] += xtmp;
           Y[(xi*yfreqs+xj)*nchannels+ci] += ytmp;
           xtmp = xtmp2;
           ytmp = ytmp2;
         }
      }
    }
  }

  /*If working at a lower sampling rate, don't take into account the last
     300 Hz to allow for different transition bands.
    For 12 kHz, we don't skip anything, because the last band already skips
     400 Hz.*/
  if(rate==48000)max_compare=BANDS[NBANDS];
  else if(rate==12000)max_compare=BANDS[ybands];
  else max_compare=BANDS[ybands]-3;
  err=0;
  for(xi=0;xi<nframes;xi++){
    double Ef;
    Ef=0;
    for(bi=0;bi<ybands;bi++){
      double Eb;
      Eb=0;
      for(xj=BANDS[bi];xj<BANDS[bi+1]&&xj<max_compare;xj++){
        for(ci=0;ci<nchannels;ci++){
          float re;
          float im;
          re=Y[(xi*yfreqs+xj)*nchannels+ci]/X[(xi*NFREQS+xj)*nchannels+ci];
          im=re-log(re)-1;
          /*Make comparison less sensitive around the SILK/CELT cross-over to
            allow for mode freedom in the filters.*/
          if(xj>=79&&xj<=81)im*=0.1F;
          if(xj==80)im*=0.1F;
          Eb+=im;
        }
      }
      Eb /= (BANDS[bi+1]-BANDS[bi])*nchannels;
      Ef += Eb*Eb;
    }
    /*Using a fixed normalization value means we're willing to accept slightly
       lower quality for lower sampling rates.*/
    Ef/=NBANDS;
    Ef*=Ef;
    err+=Ef*Ef;
  }
  err=pow(err/nframes,1.0/16);
  Q=100*(1-0.5*log(1+err)/log(1.13));
  if(Q<0){
    fprintf(stderr,"Test vector FAILS\n");
    fprintf(stderr,"Internal weighted error is %f\n",err);
    return EXIT_FAILURE;
  }
  else{
    fprintf(stderr,"Test vector PASSES\n");
    fprintf(stderr,
     "Opus quality metric: %.1f %% (internal weighted error is %f)\n",Q,err);
    return EXIT_SUCCESS;
  }
}

int main(int _argc,const char **_argv){
  FILE    *fin1;
  FILE    *fin2;
  float   *x;
  float   *y;
  size_t   xlength;
  size_t   ylength;
  size_t   xi;
  int      nchannels;
  unsigned rate;
  if(_argc<3||_argc>6){
    fprintf(stderr,"Usage: %s [-s] [-r rate2] <file1.sw> <file2.sw>\n",
     _argv[0]);
    return EXIT_FAILURE;
  }
  nchannels=1;
  if(strcmp(_argv[1],"-s")==0){
    nchannels=2;
    _argv++;
  }
  if(strcmp(_argv[1],"-r")!=0){
    fprintf(stderr,"-r is required\n");
    return EXIT_FAILURE;
  }
  rate=atoi(_argv[2]);
  _argv+=2;
  fin1=fopen(_argv[1],"rb");
  if(fin1==NULL){
    fprintf(stderr,"Error opening '%s'.\n",_argv[1]);
    return EXIT_FAILURE;
  }
  fin2=fopen(_argv[2],"rb");
  if(fin2==NULL){
    fprintf(stderr,"Error opening '%s'.\n",_argv[2]);
    fclose(fin1);
    return EXIT_FAILURE;
  }
  /*Read in the data and allocate scratch space.*/
  xlength=read_pcm16(&x,fin1,2);
  if(nchannels==1){
    for(xi=0;xi<xlength;xi++)x[xi]=.5*(x[2*xi]+x[2*xi+1]);
  }
  fclose(fin1);
  ylength=read_pcm16(&y,fin2,nchannels);
  fclose(fin2);
  if(xlength<TEST_WIN_SIZE){
    fprintf(stderr,"Insufficient sample data (%lu<%i).\n",
     (unsigned long)xlength,TEST_WIN_SIZE);
    return EXIT_FAILURE;
  }

  return verify(nchannels, x, xlength, y, rate);
}
