// Pure-C GLUT fullscreen AV demo (macOS)
// Build (dev):
//   clang -std=c11 -g -O1 -DGL_SILENCE_DEPRECATION \
//     -Wall -Wextra -Wpedantic \
//     av_glut.c -o av_glut \
//     -framework GLUT -framework OpenGL \
//     -framework AudioToolbox -framework CoreAudio
// Size-ish:
//   clang -std=c11 -Os -flto -DNDEBUG -DGL_SILENCE_DEPRECATION \
//     av_glut.c -o av_glut \
//     -framework GLUT -framework OpenGL \
//     -framework AudioToolbox -framework CoreAudio \
//     -Wl,-dead_strip,-x
//   strip -S -x av_glut

#define GL_SILENCE_DEPRECATION 1
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static double now_s(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec + ts.tv_nsec*1e-9; }

/* --------- Audio (AudioQueue, C) --------- */
typedef struct { double phase,freq,vol; int samples_left; } AudioState;
static AudioQueueRef g_q = NULL; static AudioQueueBufferRef g_bufs[3]; static AudioState g_as;

static void AQCallback(void *ud, AudioQueueRef q, AudioQueueBufferRef buf){
  (void)ud; const double sr=44100.0; int16_t *out=(int16_t*)buf->mAudioData;
  int N=(int)buf->mAudioDataBytesCapacity/2; double step=(2.0*M_PI*g_as.freq)/sr;
  for(int i=0;i<N;i++){
    if(g_as.samples_left>0){
      int atk=200, rel=200, tot=(int)(0.12*sr), age=tot-g_as.samples_left;
      double env = (age<atk) ? (double)age/atk : (g_as.samples_left<rel ? (double)g_as.samples_left/rel : 1.0);
      g_as.phase += step; if (g_as.phase>2.0*M_PI) g_as.phase-=2.0*M_PI;
      out[i]=(int16_t)(sin(g_as.phase)*32767.0*g_as.vol*0.6*env);
      g_as.samples_left--;
    } else { out[i]=0; }
  }
  buf->mAudioDataByteSize=(UInt32)(N*2); AudioQueueEnqueueBuffer(q,buf,0,NULL);
}
static int audio_init(void){
  memset(&g_as,0,sizeof(g_as)); g_as.freq=220.0; g_as.vol=0.3; g_as.samples_left=0;
  AudioStreamBasicDescription asbd={0};
  asbd.mSampleRate=44100.0; asbd.mFormatID=kAudioFormatLinearPCM;
  asbd.mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked;
  asbd.mBitsPerChannel=16; asbd.mChannelsPerFrame=1; asbd.mBytesPerFrame=2;
  asbd.mFramesPerPacket=1; asbd.mBytesPerPacket=2;
  if (AudioQueueNewOutput(&asbd, AQCallback, NULL, NULL, NULL, 0, &g_q)!=noErr || !g_q) return 0;
  const UInt32 BYTES=2048*2;
  for(int i=0;i<3;i++){ AudioQueueAllocateBuffer(g_q, BYTES, &g_bufs[i]); g_bufs[i]->mAudioDataByteSize=BYTES; memset(g_bufs[i]->mAudioData,0,BYTES); AudioQueueEnqueueBuffer(g_q,g_bufs[i],0,NULL); }
  return AudioQueueStart(g_q,NULL)==noErr;
}
static void audio_beep(double freq,double vol){ g_as.freq=freq; g_as.vol=vol; g_as.samples_left=(int)(0.12*44100.0); }
static void audio_shutdown(void){ if(g_q){ AudioQueueStop(g_q,true); AudioQueueDispose(g_q,true); g_q=NULL; } }
static double note_minor_pent_step(int k){ static const int st[]={0,3,5,7,10}; int o=k/5; int d=st[k%5]; double base=220.0*pow(2.0,o); return base*pow(2.0,d/12.0); }

/* --------- Visuals --------- */
static double g_t0=0.0; static int g_last_sec=-1; static int g_k=0;
static int g_w=1280, g_h=800;

static void draw_polygon(int sides,float angle,float radius,float r,float g,float b,float a){
  glColor4f(r,g,b,a); glBegin(GL_TRIANGLE_FAN); glVertex2f(0.f,0.f);
  for(int i=0;i<=sides;i++){ float th=angle + (float)i/(float)sides * 6.28318f; glVertex2f(cosf(th)*radius, sinf(th)*radius); }
  glEnd();
}
static void draw_lissajous(float t){
  glColor4f(1.f,1.f,1.f, 0.25f + 0.25f*sinf(0.9f*t)); glLineWidth(2.f);
  glBegin(GL_LINE_STRIP); const int N=600; float A=1.f,B=1.f,a=3.f,b=2.f,d=0.8f;
  for(int i=0;i<=N;i++){ float u=(float)i/(float)N; float x=A*sinf(a*u*6.28318f + d + 0.2f*t); float y=B*sinf(b*u*6.28318f + 0.5f*d + 0.17f*t); glVertex2f(x*0.85f, y*0.85f); }
  glEnd();
}

static void reshape(int w,int h){
  g_w=w; g_h=h; glViewport(0,0,w,h);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  double aspect=(double)w/(double)h; glOrtho(-aspect,aspect,-1,1,-1,1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
static void display(void){
  double t = now_s() - g_t0;
  if (t>75.0){ audio_shutdown(); exit(0); }

  int sec=(int)floor(t); if (sec!=g_last_sec){ g_last_sec=sec; audio_beep(note_minor_pent_step(g_k++),0.4); }

  float tf=(float)t;
  float hue=fmodf(0.12f + 0.08f*sinf(tf*0.25f) + 0.5f*sinf(tf*0.07f), 1.0f);
  float r=0.5f+0.5f*sinf((hue+0.00f)*6.28318f), g=0.5f+0.5f*sinf((hue+0.33f)*6.28318f), b=0.5f+0.5f*sinf((hue+0.66f)*6.28318f);
  glClearColor(r*0.3f, g*0.3f, b*0.3f, 1.0f); glClear(GL_COLOR_BUFFER_BIT); glLoadIdentity();

  int baseSides=3 + ((int)floor(t*0.5) % 8);
  for(int i=0;i<7;i++){
    float sc=0.9f - i*0.1f;
    float ang=tf*0.7f + i*0.6f + 0.3f*sinf(tf*0.33f + (float)i);
    float rr=fmodf(hue + i*0.07f, 1.0f);
    float cr=0.5f+0.5f*sinf((rr+0.00f)*6.28318f);
    float cg=0.5f+0.5f*sinf((rr+0.33f)*6.28318f);
    float cb=0.5f+0.5f*sinf((rr+0.66f)*6.28318f);
    draw_polygon(baseSides+(i%3), ang, sc, cr,cg,cb, 0.20f+0.08f*(float)(7-i));
  }
  draw_lissajous(tf);

  glutSwapBuffers();
}
static void timer(int v){ (void)v; glutPostRedisplay(); glutTimerFunc(16,timer,0); }

int main(int argc,char**argv){
  if (!audio_init()) return 2;
  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(g_w,g_h);
  glutCreateWindow("AV GLUT");
  glutFullScreen();                // fullscreen (esc to leave if needed)
  glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  reshape(g_w,g_h);
  g_t0=now_s();
  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutTimerFunc(16,timer,0);
  glutMainLoop();
  return 0;
}
