//Xpilot-AI Team 2012
#include <Python.h>
//from xpilot.c -EGG
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#ifndef _WINDOWS
# include <unistd.h>
# ifndef __hpux
#  include <sys/time.h>
# endif
# include <sys/param.h>
# include <netdb.h>
#endif
#ifdef _WINDOWS
# include "NT/winNet.h"
# include "NT/winClient.h"
#endif
#include "version.h"
#include "config.h"
#include "const.h"
#include "types.h"
#include "pack.h"
#include "bit.h"
#include "error.h"
#include "socklib.h"
#include "net.h"
#include "connectparam.h"
#include "protoclient.h"
#include "portability.h"
#include "checknames.h"
#include "commonproto.h"
//end from xpilot.c -EGG
//from xpclient_x11.h -EGG
#include "client.h" //originally xpclient.h -EGG
#ifdef HAVE_X11_X_H
#  include <X11/X.h>
#endif
#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#endif
#ifdef HAVE_X11_XOS_H
#  include <X11/Xos.h>
#endif
#ifdef HAVE_X11_XUTIL_H
#  include <X11/Xutil.h>
#endif
#ifdef HAVE_X11_KEYSYM_H
#  include <X11/keysym.h>
#endif
#ifdef HAVE_X11_XATOM_H
#  include <X11/Xatom.h>
#endif
#ifdef HAVE_X11_XMD_H
#  include <X11/Xmd.h>
#endif
/* X client specific headers */
#ifndef _WINDOWS
#include <X11/Xlib.h> //for X graphics -EGG
#include <X11/keysym.h> //for X keys -EGG
#endif
#include "blockbitmaps.h" //originally bitmaps.h -EGG
#include "dbuff.h"
#include "paint.h" //originally xpaint.h, moved from below xinit.h -EGG
#include "paintdata.h"
#include "record.h"
#include "widget.h"
#include "../common/keys.h"
#include "xevent.h"
//#include "xeventhandlers.h"
#include "xinit.h"
//end from xpclient_x11.h -EGG
//#include <sys/ipc.h>
//#include <sys/shm.h>
//#include <sys/mman.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include "xpclient_x11.h"
#include <math.h>
#include "../common/rules.h"
#include "../common/setup.h"
#include "configure.h"
#include "netclient.h"
#ifdef _WINDOWS
# include "../common/NT/winXKey.h" //added for XK_keys
#endif
#define COLOR(i) (i / areas) //These should also be externed from some file? /hatten
#define BASE_X(i) (((i % x_areas) << 8) + ext_view_x_offset)
#define BASE_Y(i) ((ext_view_height - 1 - (((i / x_areas) % y_areas) << 8)) - ext_view_y_offset)
#define PI_AI 3.1415926536
#define sgn(x) ((x<0)?-1:((x>0)?1:0)) //Returns the sign of a number
#define AI_MSGLEN   256 //Max length of a message 
#define AI_MSGMAX   16 //Size of (incoming) message buffer - default maxMessage is 8
#define MAX_CHECKPOINT 26 //this should be externed from some file /hatten
#ifdef _WINDOWS
# define XK_Shift_L 0xFFE1
# define XK_Control_L 0xFFE3
#endif
static PyObject* py_loop = NULL;
//Added to allow thrust etc not have to be used by a flag -hatten
KeySym pressedKeys[100];
int pressedKeyCount = 0;
#define INTTYPE 0
#define DOUBLETYPE 1
#define STRINGTYPE 2
#define BOOLTYPE 3
//TODO: support gravitypoint, but server needs to be patched first. -hatten

#define SET 0
#define UNSET 1
#define UNAVAILABLE 2
typedef struct {
  char name[64]; //maxunshieldedplayerwallbounceangle is longest w/ 35ch
  char stringValue[32];
  int intValue;
  double doubleValue;
  int status;
  int type;
} options_struct;
#define storedOptionCount 261
options_struct storedOptions[storedOptionCount];
options_struct queuedOptions[storedOptionCount];
int queuedOptionCount = 0;

int maxTurn = 16; //Added for [s/g]etMaxTurn[/Deg/Rad]

//Added for headless -EGG
int headless =0;
//Defined some stuff to avoid undefined warnings -EGG
extern int main(int argc, char* argv[]);
message_t *TalkMsg[MAX_MSGS], *GameMsg[MAX_MSGS];
score_object_t  score_objects[MAX_SCORE_OBJECTS];
//Defined selfTrackingDeg & selfHeadingDeg to avoid needing pyAI.h -EGG
double selfTrackingDeg();
double selfHeadingDeg();
double selfTrackingRad();
double selfHeadingRad();
double wallBetween();
struct AI_msg_struct {
  char body[AI_MSGLEN];
  char from[32];
  char to[32];
} AI_msg[AI_MSGMAX];
//Ship stuff -JNE
//Stores all the information that gets updated in the ship buffer -JNE
typedef struct {
  double vel;
  double velX;
  double velY;
  double trackingDeg;
  double trackingRad;
  double d;
  //int reload;
  ship_t ship;
} shipData_t;
shipData_t allShips[128][3];
int prevFrameShips = 0;

//Asteroid tracking stuff. All functions related to tracking will be more or less
//straight copy-pastes of similar shot functions. All tracking should be 
//generalized for enemies, shots, asteroids, missiles and mines. TODO
typedef struct {
  double velX;
  double velY;
  int angVel; 
  int age;
  int alert;      /*MIN(yatx + timex, xaty + timey) */
} object_t;

#define AIASTEROID_MAX 200 //TODO: figure out a good number
struct AIasteroid_struct {
  asteroid_t data;
  object_t ai;
} AIasteroid[2][AIASTEROID_MAX];
int asteroidCount[2] = {0, 0};

struct AImissile_struct {
  missile_t data;
  object_t ai;
} AImissile[2][AIASTEROID_MAX];
int missileCount[2] = {0, 0};

struct AIlaser_struct {
  laser_t data;
  object_t ai;
} AIlaser[2][AIASTEROID_MAX];
int laserCount[2] = {0, 0};

struct AIball_struct {
  ball_t data;
  object_t ai;
} AIball[2][AIASTEROID_MAX];
int ballCount[2] = {0, 0};

/*struct AIship_struct {
  ship_t data;
  object_t ai;
  } AIship[2][AIASTEROID_MAX];
  int shipCount[2] = {0, 0};*/

#define AISHOT_MAX    100 /*max number of shots */
struct AIshot_struct {
  int x;
  int y;
  int color;
  object_t ai;
} AIshot[2][AISHOT_MAX];
int shotCount[2] = {0, 0};

struct AImine_struct {
  mine_t data;
  object_t ai;
} AImine[2][AIASTEROID_MAX];
int mineCount[2] = {0, 0};

struct AIitem_struct {
  itemtype_t data;
  object_t ai;
  int random;
} AIitem[2][AIASTEROID_MAX];
int itemCount[2] = {0, 0};

struct AIradar_struct {
  radar_t data;
  object_t ai;
} AIradar[2][AIASTEROID_MAX];
//int radarCount[2] = {0, 0}; //why did I need this? /hatten

struct AIdebris_struct {
  debris_t data;
  object_t ai;
} AIdebris[2][AIASTEROID_MAX];
int debrisCount[2] = {0, 0};

struct AIwreckage_struct {
  wreckage_t data;
  object_t ai;
} AIwreckage[2][AIASTEROID_MAX];
int wreckageCount[2] = {0, 0};

int AI_delaystart;
int AIshot_toggle;
int AI_alerttimemult;

double AI_radToDeg(double rad) {
  return rad * 180.0 / PI_AI;
}
double AI_radToXdeg(double rad) {
  return rad * 64.0 / PI_AI;
}
double AI_degToRad(double deg) {
  return deg * PI_AI / 180.0;
}
double AI_degToXdeg(double deg) {
  return deg * 64.0 / 180.0;
}
double AI_xdegToRad(double xdeg) {
  return xdeg * PI_AI / 64.0;
}
double AI_xdegToDeg(double xdeg) {
  return xdeg * 180.0 / 64.0;
}
double AI_distance(int x1, int y1, int x2, int y2) {
  return sqrt((double)(pow((x2-x1),2)+pow((y2-y1),2)));
}
double AI_speed(double velX, double velY) {
  return sqrt((double)(pow((velX),2)+pow((velY),2)));
}
int AI_wrap(int a, int b, int size) {
  return abs(a-b) < size/2 ? b : a<b ? b-size : b+size;
}
void wrapWhole(int x1, int y1, int* x2, int* y2, int xSize, int ySize, double* best) {
  int xMult, yMult, xBest, yBest;
  *best=-1;
  double dist;
  for (xMult=-1; xMult<2; xMult++) {
    for (yMult=-1; yMult<2; yMult++) {
      dist=AI_distance(x1, y1, *x2 + xMult*xSize, *y2 + yMult*ySize);
      if (dist < *best || *best == -1) {
        *best=dist;
        xBest=*x2 + xMult * xSize;
        yBest=*y2 + yMult * ySize;
      }
    }
  }
  *x2=xBest;
  *y2=yBest;
  return;
}
//Asteroid tracking stuff. Should be merged with shot tracking stuff (and ship,missiles,mines, tanks, wreckage, laser and radar tracking!). -hatten

void AIobject_calcAlert(object_t *object, int x, int y) {
  float A, B, C, BAC;
  int newx1, newx2, newy1, newy2, itime, velX, velY;
  double idist;

  if (object->age == 0)
    return;
  velX = object->velX;
  velY = object->velY;
  A = sqr(vel.y - velY) + sqr(vel.x - velX);
  B = 2 * ((pos.y - y) * (vel.y - velY) + (pos.x - x) * (vel.x - velX));
  C = sqr(pos.x - x) + sqr(pos.y - y);
  BAC = sqr(B) - 4 * A * C;

  if (BAC >= 0) {
    BAC = (-1 * B + sqrt(BAC));
    if ((BAC / (2 * A)) < 0)
      BAC = (-1 * B - sqrt(sqr(B) - 4 * A * C));
    itime = BAC / (2 * A);
  }
  else {
    itime = (-1 * B) / (2 * A);
  }

  newx1 = pos.x + vel.x * itime;
  newx2 = x     + velX  * itime;
  newy1 = pos.y + vel.y * itime;
  newy2 = y     + velY  * itime;
  wrapWhole(newx1, newy1, &newx2, &newy2, Setup->width, Setup->height, &idist);
  /*if (itime <= 0)
    object->alert = 30000;
    else*/
  object->alert = abs((int)idist + (int) (itime * AI_alerttimemult));
}
int AIobject_calcVel(int x, int x1, int y, int y1, int rotation, int rotation1,
    object_t *current, object_t *past) {
  int angVel;
  double velX, velY;
  if (rotation != -1) {
    angVel = (rotation - rotation1 + 128) % 128;
    if (angVel < 2 || angVel > 8 )
      return 0;
  }
  else
    angVel = -1;

  velX = x - AI_wrap(x, x1, Setup->width);
  velY = y - AI_wrap(y, y1, Setup->height);

  if (past->age == 0) {
    current->angVel = angVel;
    current->velX = velX;
    current->velY = velY;
    current->age = 1;
    AIobject_calcAlert(current, x, y);
    return 2;
  }

  if (angVel != past->angVel && angVel != -1)
    return 0;
  if (abs(velX - past->velX) > 3)
    return 0;
  if (abs(velY - past->velY) > 3)
    return 0;
  current->angVel = angVel;
  current->velX   = (past->velX+velX) / 2.0;
  current->velY   = (past->velY+velY) / 2.0;
  current->age    = past->age +1;
  AIobject_calcAlert(current, x, y);
  return 1;
}
void AIasteroid_calcVel() {
  int i, j, found, asteroidCountPreCopies;
  //First find objects in the previus ticks we have data on
  //and find objects in the current tick for them
  for (j=0; j < asteroidCount[1]; j++) {
    if (AIasteroid[1][j].ai.age > 0) {
      found=0;
      for (i=0; i < asteroidCount[0]; i++) {
        if (AIasteroid[0][i].data.size != AIasteroid[1][j].data.size)
          continue;
        if (AIasteroid[0][i].data.type != AIasteroid[1][j].data.type)
          continue;
        found = AIobject_calcVel(
            AIasteroid[0][i].data.x, AIasteroid[1][j].data.x,
            AIasteroid[0][i].data.y, AIasteroid[1][j].data.y,
            AIasteroid[0][i].data.rotation, AIasteroid[1][j].data.rotation,
            &AIasteroid[0][i].ai, &AIasteroid[1][j].ai);
        if (found==1)
          break;
      }
    }
  }
  //Then go through the remaining objects in the previous ticks
  //and make copies of the remaining objects in the current tick
  //and create objects with data from that.
  //This will cover all cornercases, and give the user the option
  //to play it safe and accord every possible object (age 1)
  //or only those that are sure to exist (age >2)
  for (j=0; j < asteroidCount[1]; j++) {
    asteroidCountPreCopies = asteroidCount[0];
    if (AIasteroid[1][j].ai.age == 0) {
      found = 0;
      for (i = 0; i < asteroidCountPreCopies && asteroidCount[0] < AIASTEROID_MAX; i++) {
        if (AIasteroid[0][i].ai.age != 0)
          continue;
        if (AIasteroid[0][i].data.size != AIasteroid[1][j].data.size)
          continue;
        if (AIasteroid[0][i].data.type != AIasteroid[1][j].data.type)
          continue;
        AIasteroid[0][asteroidCount[0]] = AIasteroid[0][i];
        found = AIobject_calcVel(
            AIasteroid[0][asteroidCount[0]].data.x, AIasteroid[1][j].data.x,
            AIasteroid[0][asteroidCount[0]].data.y, AIasteroid[1][j].data.y,
            AIasteroid[0][asteroidCount[0]].data.rotation, AIasteroid[1][j].data.rotation,
            &AIasteroid[0][asteroidCount[0]].ai, &AIasteroid[1][j].ai);
        if (found==2 && asteroidCount[0] <= AIASTEROID_MAX)
          asteroidCount[0]++;
      }
    }
  }
}
void AIasteroid_refresh() {
  int i;
  asteroidCount[1] = asteroidCount[0];
  if (num_asteroids > AIASTEROID_MAX) {
    printf("ERROR: There are %d asteroids on the screen, the API is unable to process more than %d!\n",
        num_asteroids,AIASTEROID_MAX);
    asteroidCount[0] = 100;
  }
  else
    asteroidCount[0] = num_asteroids;
  for (i=0; i < asteroidCount[1]; i++)
    AIasteroid[1][i] = AIasteroid[0][i];
  for (i=0; i < asteroidCount[0]; i++) {
    AIasteroid[0][i].data = asteroid_ptr[i];
    AIasteroid[0][i].ai.age = 0;
  }
  AIasteroid_calcVel();
}
void AIshot_calcVel() {
  int i, j, found, shotCountPreCopies;
  for (j=0; j < shotCount[1]; j++) {
    if (AIshot[1][j].ai.age > 0) {
      found=0;
      for (i=0; i < shotCount[0]; i++) {
        found = AIobject_calcVel(
            AIshot[0][i].x, AIshot[1][j].x,
            AIshot[0][i].y, AIshot[1][j].y,
            -1, -1,
            &AIshot[0][i].ai, &AIshot[1][j].ai);
        if (found==1)
          break;
      }
    }
  }
  for (j=0; j < shotCount[1]; j++) {
    shotCountPreCopies = shotCount[0];
    if (AIshot[1][j].ai.age == 0) {
      found = 0;
      for (i = 0; i < shotCountPreCopies && shotCount[0] < AISHOT_MAX; i++) {
        if (AIshot[0][i].ai.age != 0)
          continue;
        AIshot[0][shotCount[0]] = AIshot[0][i];
        found = AIobject_calcVel(
            AIshot[0][shotCount[0]].x, AIshot[1][j].x,
            AIshot[0][shotCount[0]].y, AIshot[1][j].y,
            -1, -1,
            &AIshot[0][shotCount[0]].ai, &AIshot[1][j].ai);
        if (found==2 && shotCount[0] <= AIASTEROID_MAX)
          shotCount[0]++;
      }
    }
  }
}
void AIshot_refresh() {
  int i, x_areas, y_areas, areas, max_, color;

  //update old slots
  shotCount[1] = shotCount[0];
  for (i=0; i< shotCount[1]; i++)
    AIshot[1][i] = AIshot[0][i];
  //update new slots
  shotCount[0] = 0;
  x_areas = (active_view_width + 255) >> 8;
  y_areas = (active_view_height + 255) >> 8;
  areas = x_areas * y_areas;
  max_ = areas * (num_spark_colors >= 3 ? num_spark_colors : 4);
  for (i = 0; i < max_; i++) {
    int x, y, j;
    if (num_fastshot[i] > 0) {
      x = BASE_X(i);
      y = BASE_Y(i);
      color = COLOR(i);
      for (j = 0; j < num_fastshot[i]; j++) {
        if (shotCount[0] < AISHOT_MAX) {
          //WINSCALE is a very mysterious function that I do not know what it does. My guess is that
          //it is used to counteract the zooming that a player can do - something that is rarely done
          //with AI's.
          AIshot[0][shotCount[0]].x = pos.x - ext_view_width  / 2 + WINSCALE(x + fastshot_ptr[i][j].x);
          AIshot[0][shotCount[0]].y = pos.y + ext_view_height / 2 - WINSCALE(y - fastshot_ptr[i][j].y);
          AIshot[0][shotCount[0]].color = color;
          AIshot[0][shotCount[0]].ai.age = 0;
        }
        shotCount[0]++;
      }
    }
  }
  if (shotCount[0] > AISHOT_MAX) {
    printf("ERROR: There are %d shots on the screen, the API is unable to process more than %d!\n",
        shotCount[0],AISHOT_MAX);
    shotCount[0] = 100;
  }
  AIshot_calcVel();
}
void AIitem_calcVel() {
  int i, j, found, itemCountPreCopies;
  double randomItemProb;
  i = getOption("randomitemprob");
  if (i >= 0 && storedOptions[i].status == SET) {
    randomItemProb = storedOptions[i].doubleValue;
  }
  else
    randomItemProb = 1;
  //First find objects in the previus ticks we have data on
  //and find objects in the current tick for them
  for (j=0; j < itemCount[1]; j++) {
    if (AIitem[1][j].ai.age > 0) {
      found=0;
      for (i=0; i < itemCount[0]; i++) {
        if (randomItemProb == 0.0 && AIitem[0][i].data.type != AIitem[1][j].data.type) {
          continue;
        }
        if (AIobject_calcVel(
              AIitem[0][i].data.x, AIitem[1][j].data.x,
              AIitem[0][i].data.y, AIitem[1][j].data.y,
              -1, -1,
              &AIitem[0][i].ai, &AIitem[1][j].ai) == 1) {
          if (AIitem[0][i].data.type != AIitem[1][j].data.type)
            AIitem[0][i].random = 1; //Somewhat sketchy
          break;
        }
      }
    }
  }
  //Then go through the remaining objects in the previous ticks
  //and make copies of the remaining objects in the current tick
  //and create objects with data from that.
  //This will cover all cornercases, and give the user the option
  //to play it safe and accord every possible object (age 1)
  //or only those that are sure to exist (age >2)
  for (j=0; j < itemCount[1]; j++) {
    itemCountPreCopies = itemCount[0];
    if (AIitem[1][j].ai.age == 0) {
      found = 0;
      for (i = 0; i < itemCountPreCopies && itemCount[0] < AIASTEROID_MAX; i++) {
        if (AIitem[0][i].ai.age != 0)
          continue;
        AIitem[0][itemCount[0]] = AIitem[0][i];
        found = AIobject_calcVel(
            AIitem[0][itemCount[0]].data.x, AIitem[1][j].data.x,
            AIitem[0][itemCount[0]].data.y, AIitem[1][j].data.y,
            -1, -1,
            &AIitem[0][itemCount[0]].ai, &AIitem[1][j].ai);
        if (found==2 && itemCount[0] <= AIASTEROID_MAX)
          itemCount[0]++;
      }
    }
  }
}
void AIitem_refresh() {
  int i;
  itemCount[1] = itemCount[0];
  if (num_itemtype > AIASTEROID_MAX) {
    printf("ERROR: There are %d items on the screen, the API is unable to process more than %d!\n",
        num_itemtype,AIASTEROID_MAX);
    itemCount[0] = 100;
  }
  else
    itemCount[0] = num_itemtype;
  for (i=0; i < itemCount[1]; i++)
    AIitem[1][i] = AIitem[0][i];
  for (i=0; i < itemCount[0]; i++) {
    AIitem[0][i].data = itemtype_ptr[i];
    AIitem[0][i].ai.age = 0;
  }
  AIitem_calcVel();
}

//
//END OF tracking CODE
//

//Reload tracker
int reload = 0;
//From xpilot-ng's event.c to make key functions easier -EGG
typedef int xp_keysym_t;
void Keyboard_button_pressed(xp_keysym_t ks) {
  bool change = false;
  keys_t key;
  for (key = Lookup_key(NULL, ks, true);
      key != KEY_DUMMY;
      key = Lookup_key(NULL, ks, false)) {
    change |= Key_press(key);
  }
  if (change)
    Net_key_change();
}
void Keyboard_button_released(xp_keysym_t ks) {
  bool change = false;
  keys_t key;
  for (key = Lookup_key(NULL, ks, true);
      key != KEY_DUMMY;
      key = Lookup_key(NULL, ks, false)) {
    change |= Key_release(key);
  }
  if (change)
    Net_key_change();
}
void press_key(KeySym key) {
  int i;
  for (i=0; i < pressedKeyCount; i++) {
    if (pressedKeys[i] == key) {
      return;
    }
  }
  Keyboard_button_pressed(key);
  pressedKeys[pressedKeyCount] = key;
  pressedKeyCount++;
}
void press_release_key(xp_keysym_t ks) {
  Keyboard_button_pressed(ks);
  Keyboard_button_released(ks);
}
//END from event.c
//All button press methods are documented on 
int getLag(void) {
  return packet_lag;
}
void turnLeft(void) {
  press_key(XK_a);
}
void turnRight(void) {
  press_key(XK_s);
}

void turn(double xdeg) {
  //used by all turn() and turnTo() functions -hatten
  while (xdeg > 64)
    xdeg -= 128;
  while (xdeg < -64)
    xdeg += 128;
  if (xdeg > maxTurn)
    xdeg = maxTurn;
  else if (xdeg < -maxTurn)
    xdeg = -maxTurn;
  if (xdeg)
    Send_pointer_move((int)round(-xdeg));
}
void turnXdeg(double xdeg) {
  turn(xdeg);
}
void turnToXdeg(double xdeg) {
  turn((xdeg-(double)selfHeading()));
}
void turnDeg(double deg) {
  turn(AI_degToXdeg(deg));
}
void turnToDeg(double deg) {
  turn(AI_degToXdeg(deg-selfHeadingDeg()));
}
void turnRad(double rad) {
  turn(AI_radToXdeg(rad));
}
void turnToRad(double rad) {
  turn(AI_radToXdeg(rad-selfHeadingRad()));
}

int setMaxTurn(double xdeg) {
  if (xdeg < 0) {
    return 1;
  }
  if (xdeg > 64) {
    xdeg = 64;
  }
  maxTurn=xdeg;
  return 0;
}
int setMaxTurnXdeg(double max) {
  return setMaxTurn(max);
}
int setMaxTurnDeg(double max) {
  return setMaxTurn(AI_degToXdeg(max));
}
int setMaxTurnRad(double max) {
  return setMaxTurn(AI_radToXdeg(max));
}
double getMaxTurnXdeg(void) {
  return maxTurn;
}
double getMaxTurnDeg(void) {
  return AI_xdegToDeg(maxTurn);
}
double getMaxTurnRad(void) {
  return AI_xdegToRad(maxTurn);
}
void thrust(void) {
  press_key(XK_Shift_L);
}

//Sets the player's turnspeed. -EGG
//Will not take effect until the player STARTS turning AFTER this is called. -EGG
//Parameters: int for speed, min = 0, max = 64. -EGG
int setTurnSpeed(double speed) {
  if ( power < 4 || power > 64) {
    return 1;
  }
  Send_turnspeed(speed);
  Config_redraw();
  control_count = CONTROL_DELAY;
  return 0;
}
int setPower(double s) {
  if (s<5 || s > 55) {
    return 1;
  }
  Send_power(s);
  Config_redraw();
  control_count = CONTROL_DELAY;
  return 0;
}
int setTurnResistance(double s) {
  if (s<0 || s > 1) {
    return 1;
  }
  Send_turnresistance(s);
  Config_redraw();
  control_count = CONTROL_DELAY;
  return 0;
}
//~ //End movement methods -JNE
//~ //Shooting methods -JNE
void fireShot(void) {
  press_release_key(XK_Return);
  if (reload==0) {
    reload = storedOptions[getOption("firerepeatrate")].intValue - 1; //TODO: Foolproof
  }
}
void fireMissile(void) {
  press_release_key(XK_backslash);
}
void fireTorpedo(void) {
  press_release_key(XK_apostrophe);
}
void fireHeat(void) {
  press_release_key(XK_semicolon);
}
void dropMine(void) {
  press_release_key(XK_Tab);
}
void detachMine(void) {
  press_release_key(XK_bracketright);
}
void detonateMines(void) {
  press_release_key(XK_equal);
}
void fireLaser(void) {
  press_key(XK_slash);
}

//End shooting methods -JNE
//Item usage methods -JNE
void tankDetach(void) {
  press_release_key(XK_r);
}
void cloak(void) {
  press_release_key(XK_BackSpace);
}
void ecm(void) {
  press_release_key(XK_bracketleft);
}
void transporter(void) {
  press_release_key(XK_t);
}
void tractorBeam(void) {
  press_key(XK_comma);
}
void pressorBeam(void) {
  press_key(XK_period);
}
void phasing(void) {
  press_release_key(XK_p);
}
void shield(void) {
  press_key(XK_space);
}
void emergencyShield(void) {
  press_release_key(XK_g);
}
void hyperjump(void) {
  press_release_key(XK_q);
}
void nextTank(void) {
  press_release_key(XK_e);
}
void prevTank(void) {
  press_release_key(XK_w);
}
void toggleAutopilot(void) {
  press_release_key(XK_h);
}
void emergencyThrust(void) {
  press_release_key(XK_j);
}
void deflector(void) {
  press_release_key(XK_0);
}
void selectItem(void) {
  press_release_key(XK_KP_0);
}
void loseItem(void) {
  press_release_key(XK_KP_Decimal);
}
//End item usage methods -JNE
//Lock methods -JNE All changed -hatten
void lockNext(void) {
  press_key(XK_Right);
}
void lockPrev(void) {
  press_key(XK_Left);
}
void lockClose(void) {
  press_key(XK_Up);
}
void lockNextClose(void) {
  press_key(XK_Down);
}
int loadLock(int lock) {
  switch (lock) {
    case 1:
      press_key(XK_5);
      break;
    case 2:
      press_key(XK_6);
      break;
    case 3:
      press_key(XK_7);
      break;
    case 4:
      press_key(XK_8);
      break;
    default:
      return 1;
  }
  return 0;
}
int saveLock(int lock) {
  if (lock > 0 && lock < 5) {
    press_key(XK_grave);
    return loadLock(lock);
  }
}
int getLockId(void) {
  return lock_id;
}
//end new lock methods
//Modifier methods -JNE
void toggleNuclear(void) {
  press_release_key(XK_n);
}
void togglePower(void) {
  press_release_key(XK_b);
}
void toggleVelocity(void) {
  press_release_key(XK_v);
}
void toggleCluster(void) {
  press_release_key(XK_c);
}
void toggleMini(void) {
  press_release_key(XK_x);
}
void toggleSpread(void) {
  press_release_key(XK_z);
}
void toggleLaser(void) {
  press_release_key(XK_l);
}
void toggleImplosion(void) {
  press_release_key(XK_i);
}
void toggleUserName(void) {
  press_release_key(XK_u);
}
int loadModifier(int i) {
  switch (i) {
    case 1:
      press_release_key(XK_1);
      break;
    case 2:
      press_release_key(XK_2);
      break;
    case 3:
      press_release_key(XK_3);
      break;
    case 4:
      press_release_key(XK_4);
      break;
    default:
      return 1;
  }
  return 0;
}
int saveModifier(int i) {
  if (i > 0 && i < 5) {
    press_key(XK_grave);
    return loadModifier(i);
  }
}
void clearModifiers(void) {
  press_release_key(XK_k);
}
//End modifier methods -JNE
//Map features -JNE
void connector(void) {
  press_key(XK_Control_L);
}
void dropBall(void) {
  press_release_key(XK_d);
}
void refuel(void) {
  press_key(XK_Control_L);
}
//End map features -JNE
//Other options -JNE

void keyHome(void) {
  press_release_key(XK_Home);
}
void selfDestruct(void) {
  press_release_key(XK_End);
}
void pauseAI(void) {
  press_release_key(XK_Pause);
}
void swapSettings(void) {
  press_release_key(XK_Escape);
}
void quitAI(void) {
  Net_cleanup();
  Client_cleanup();
  exit(0);
}
void talkKey(void) {
  press_release_key(XK_m);
}
void toggleShowMessage(void) {
  press_release_key(XK_KP_9);
}
void toggleShowItems(void) {
  press_release_key(XK_KP_8);
}
void toggleCompass(void) {
  press_release_key(XK_KP_7);
}
void repair(void) {
  press_release_key(XK_f);
}
void reprogram(void) {
  press_key(XK_grave);
}
int getMaxMsgs(void) {
  return maxMessages;
}
int setMaxMsgs(int var) {
  if (var < 1 || var > 15) {
    return 1;
  }
  maxMessages=var;
  return 0;
}
//Talk Function, can't be called too frequently or client will flood - JTO
void talk(char* talk_str) {
  Net_talk(talk_str);
  Send_talk(); //cut down number of frames we had to wait for the
  //message to be sent from 72 to 3. -hatten
}
char* scanTalkMsg(int id) {
  if (id < MAX_MSGS) {
    return TalkMsg[id]->txt;
  }
  return "invalid id";
}
int removeTalkMsg(int i) {
  if (i<MAX_MSGS) {
    strlcpy(TalkMsg[i]->txt, "", MSG_LEN);
    TalkMsg[i]->len=0;
    return 1;
  }
  return 0;
}
char* scanGameMsg(int id) {
  if (id < MAX_MSGS) {
    return TalkMsg[id]->txt;
  }
  return "invalid id";
}
int removeGameMsg(int i) {
  if (i<MAX_MSGS) {
    strlcpy(TalkMsg[i]->txt, "", MSG_LEN);
    TalkMsg[i]->len=0;
    return 1;
  }
  return 0;
}
//Self properties -JNE
int selfX(void) {
  return pos.x;
}
int selfY(void) {
  return pos.y;
}
int selfVelX(void) {
  return vel.x;
}
int selfVelY(void) {
  return vel.y;
}
double selfSpeed(void) {
  return sqrt(pow(vel.x,2)+pow(vel.y,2));
}
int lockHeadingXdeg(void) {
  return lock_dir;
}
int lockHeadingDeg(void) {
  return (double)lock_dir*2.8125;
}
int lockHeadingRad(void) {
  return (double)lock_dir*.049087;
}
int selfLockDist(void) {
  return lock_dist;
}
int selfReload(void) {
  return reload;
}
//Gets the player's ID, returns an int. -EGG
int selfID() { //DO NOT CHANGE, NEEDED IN ORDER FOR addNewShip to work -JRA
  if (self != NULL)
    return self->id;
  return -1;
}

static PyObject* py_selfID(PyObject* pySelf, PyObject* args) { 
  if (self != NULL)
    return Py_BuildValue("i",self->id);
  return Py_BuildValue("i",-1);
}
//Returns 1 if the player is alive, 0 if they are not. -EGG
int selfAlive(void) {
  return selfVisible;
}
//Returns the player's team (int). -EGG
static PyObject* py_selfTeam(PyObject* pySelf, PyObject* args) {
  if (self != NULL)
    return Py_BuildValue("i",self->team);
  return Py_BuildValue("i",-1);
}
//Returns the player's lives remaining (if there is a life limit) or the number of lives spent (int). -EGG
static PyObject* py_selfLives(PyObject* pySelf, PyObject* args) {
  if (self != NULL)
    return Py_BuildValue("i",self->life);
  return Py_BuildValue("i",-1);
}
double selfTrackingRad() {  //returns the player's tracking in radians  -JNE  //DO NOT CHANGE, NEEDED IN ORDER FOR selfTrackingDeg to work -JRA
  if (vel.y == 0 && vel.x == 0) return selfHeadingRad(); //fix for NaN -EGG -CJG
  return atan2((double)vel.y,(double)vel.x);
}

double selfTrackingDeg() {  //returns the player's tracking in degrees -JNE //DO NOT CHANGE, NEEDED IN ORDER FOR aimdir &  AIshot_addtobuffer to work -JRA
  //if (vel.y == 0 && vel.x == 0) return selfHeadingDeg(); //fix for NaN -EGG -CJG
  return AI_radToDeg(selfTrackingRad());
}
int selfHeading() { //used by py_turnTo() -hatten
  return heading;
}
double selfHeadingDeg() {   //returns the player's heading in degrees -JNE //DO NOT CHANGE, NEEDED IN ORDER FOR turnToDeg to work -JRA
  return (double)heading*2.8125;
}
double selfHeadingRad() {
  return (double)heading*.049087;
}
static PyObject* py_hud(PyObject* pySelf, PyObject* args) {         //if the HUD is displaying a name, return it  -JNE
  int i; 
  if (!PyArg_ParseTuple(args, "i", &i)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if ( i < MAX_SCORE_OBJECTS) {
    if (score_objects[i].hud_msg_len>0) {
      return Py_BuildValue("s",score_objects[i].hud_msg);
    }
  }
  return Py_BuildValue("s","");
}
static PyObject* py_hudScore(PyObject* pySelf, PyObject* args) {        //if the HUD is displaying a score, return it -JNE
  int i;
  if (!PyArg_ParseTuple(args, "i", &i)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (i < MAX_SCORE_OBJECTS) {
    if (score_objects[i].hud_msg_len>0) {
      return Py_BuildValue("s",score_objects[i].msg);
    }
  }
  return Py_BuildValue("s","");
}
static PyObject* py_hudTimeLeft(PyObject* pySelf, PyObject* args) {      //returns how much time the HUD will keep displaying a score for, in seconds -JNE
  int i;
  if (!PyArg_ParseTuple(args, "i", &i)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (i<MAX_SCORE_OBJECTS) {
    if (score_objects[i].hud_msg_len>0) { 
      return (Py_BuildValue("i",100-score_objects[i].count));
    }
  }
  return Py_BuildValue("i",0);
}
//Gets the player's turnspeed, returns a double. -EGG
int getTurnSpeed(void) {
  return turnspeed;
}
int getPower(void) {
  return power;
}
int getTurnResistance(void) {
  return turnresistance;
}
//Returns 1 if the player's shield is on, 0 if it is not, -1 if player is not alive. -EGG
static PyObject* py_selfShield(PyObject* pySelf, PyObject* args) {
  int i;
  for (i=0;i<num_ship;i++) if ((self != NULL) && (ship_ptr[i].id==self->id)) return Py_BuildValue("i",(int)ship_ptr[i].shield);
  return Py_BuildValue("i",-1);
}
//Returns the player's username (string). -EGG
static PyObject* py_selfName(PyObject* pySelf, PyObject* args) {
  if (self != NULL) return Py_BuildValue("s",self->name);
}
//Returns the player's score (double). -EGG
static PyObject* py_selfScore(PyObject* pySelf, PyObject* args) {
  if (self != NULL) return Py_BuildValue("d",self->score);
}
static PyObject* py_selfItem(PyObject* pySelf, PyObject* args) {
  int i;
  if (!PyArg_ParseTuple(args, "i", &i)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (i < 0 || i > 20) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter, must be between 0 and 20");
    return NULL;
  }
  else if (i == 0)
    return Py_BuildValue("i",fuelSum);
  else
    return Py_BuildValue("i",numItems[i]);
}
int selfFuel(void) {
  return fuelSum;
}
int selfFuelMax(void) {
  return fuelMax;
}
int selfFuelCurrent(void) {
  return fuelCurrent;
}
//TODO: currentTank?
//numTanks is _never_ set in the program
//#define FUEL_MASS(f)    ((f)*0.005/FUEL_SCALE_FACT
static PyObject* py_selfMass(PyObject* pySelf, PyObject* args) {
  int i;
  double shipMass = 20, minItemMass=1, itemMass=0;
  i = getOption("shipmass");
  if (i>=0) {
    shipMass = storedOptions[i].doubleValue;
  }
  i = getOption("minitemmass");
  if (i>=0) {
    minItemMass = storedOptions[i].doubleValue;
  }
  double fuelMass = FUEL_MASS(fuelSum);
  for (i = 1;i < 20;i++) {
    itemMass += numItems[i]*minItemMass;
  }
  itemMass += numItems[20]*shipMass/14; //ARMOR_MASS=ShipMass/14

  return Py_BuildValue("d", shipMass+itemMass+fuelMass);
}
//pl->emptymass+FUEL_MASS(pl->fuel.sum+sum_item_mass
//End self properties -JNE
static PyObject* py_closestRadarId(PyObject* pySelf, PyObject* args) {
  int i, id, x, y;
  double best = -1, dist = -1;
  for (i = selfVisible;i < num_radar;i++) {
    x = radar_ptr[i].x;
    y = radar_ptr[i].y;
    wrapWhole(radar_ptr[0].x,radar_ptr[0].y,&x,&y, RadarWidth, RadarHeight, &dist);

    if ((dist < best) || (best == -1)) { 
      best = dist;                       //update distance
      id = i;             //update value to be returned
    }
  }
  if (best ==-1) //If so, there are no enemies (alive).
    return Py_BuildValue("i",-1);
  return Py_BuildValue("i",id);
}
static PyObject* py_radarX(PyObject* pySelf, PyObject* args) {
  //returns X coordinate of specified enemy -hatten
  int id;
  if (!PyArg_ParseTuple(args, "i", &id)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (id < num_radar) {
    return Py_BuildValue("i", radar_ptr[id].x);
  }
  return Py_BuildValue("i", -1);
}
static PyObject* py_radarY(PyObject* pySelf, PyObject* args) {
  //returns X coordinate of specified enemy -hatten
  int id;
  if (!PyArg_ParseTuple(args, "i", &id)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (id < num_radar) {
    return Py_BuildValue("i", radar_ptr[id].y);
  }
  return Py_BuildValue("i", -1);
}
static PyObject* py_radarType(PyObject* pySelf, PyObject* args) {
  int id;
  if (!PyArg_ParseTuple(args, "i", &id)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (id < num_radar) {
    return Py_BuildValue("i", radar_ptr[id].size);
  }
  return Py_BuildValue("i", -1);
}
int radarCount(void) {
  return num_radar;
}
int radarHeight(void) {
  return RadarHeight;
}
int radarWidth(void) {
  return RadarWidth;
}
int itemCountScreen(void) {
  return itemCount[0];
}
static PyObject* py_itemX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("i",AIitem[0][idx].data.x);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("i",AIitem[0][idx].data.y);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemType(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("i",AIitem[0][idx].data.type);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemRandom(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("i",AIitem[0][idx].random);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemVelX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("d",AIitem[0][idx].ai.velX);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemVelY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("d",AIitem[0][idx].ai.velY);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemAge(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0])
    return Py_BuildValue("i",AIitem[0][idx].ai.age);
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemDist(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < itemCount[0]) {
    int x = AI_wrap(pos.x, AIitem[0][idx].data.x, Setup->width);
    int y = AI_wrap(pos.y, AIitem[0][idx].data.y, Setup->height);
    return Py_BuildValue("d",AI_distance(pos.x, pos.y, x, y));
  }
  else
    PyErr_SetString(PyExc_IndexError, "No item with that id");
  return NULL;
}
static PyObject* py_itemSpeed(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIitem[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That item is still fresh");
    return NULL;
  }
  else if (idx >= itemCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No item with that id");
    return NULL;
  }
  else
    return Py_BuildValue("d",AI_speed(AIitem[0][idx].ai.velX, AIitem[0][idx].ai.velY));
}
static PyObject* py_itemTrackingDeg(PyObject* pySelf, PyObject* args) {
  int idx;
  int value;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIitem[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That item is still fresh");
    return NULL;
  }
  else if (idx >= itemCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No item with that id");
    return NULL;
  }
  else {
    int velX = AIitem[0][idx].ai.velX;
    int velY = AIitem[0][idx].ai.velY;
    if (velX == 0 && velY == 0)
      return Py_BuildValue("i", 0);
    return Py_BuildValue("d", AI_radToDeg(atan2(velY, velX)));
  }
}
static PyObject* py_itemTrackingRad(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIitem[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That item is still fresh");
    return NULL;
  }
  if (idx >= itemCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No item with that id");
    return NULL;
  }
  else {
    int velX = AIitem[0][idx].ai.velX;
    int velY = AIitem[0][idx].ai.velY;
    if (velX == 0 && velY == 0)
      return Py_BuildValue("i", 0);
    return Py_BuildValue("d", atan2(velY, velX));
  }
}
//Start wrap helper functions -JNE
//Checks if the map wraps between two x or y coordinates; if it does, it returns a usable value for the first coordinate -JNE
//May glitch if the map is smaller than ext_view_width and height -JNE
int wrapX(int firstX, int selfX) { //DO NOT CHANGE -JRA
  int tempX;
  tempX = firstX - selfX;
  if (abs(tempX)>ext_view_width) {
    if (firstX > selfX) {
      firstX = firstX - Setup->width;
    }
    else {
      firstX = firstX + Setup->width;
    }
  }
  return firstX;
}
int wrapY(int firstY, int selfY) { //DO NOT CHANGE -JRA
  int tempY;
  tempY = firstY - selfY;
  if (abs(tempY)>ext_view_height) {
    if (firstY > selfY) {
      firstY = firstY - Setup->height;
    }
    else {
      firstY = firstY + Setup->height;
    }
  }
  return firstY;
}
//End wrap helper functions -JNE
int pausedCountServer(void) {
  int i,sum=0;
  for (i=0;i<num_others;i++)
    if (Others[i].mychar == 'P')
      sum++;
  return sum;
}
int tankCountServer(void) {
  int i,sum=0;
  for (i=0;i<num_others;i++)
    if (Others[i].mychar == 'T')
      sum++;
  return sum;
}
int playerCountServer(void) {
  return num_others-pausedCountServer()-tankCountServer();
}
int otherCountServer(void) {
  return num_others;
}
static PyObject* py_enemyIdx(PyObject* pySelf, PyObject* args) {
  int id;
  if (!PyArg_ParseTuple(args, "i", &id)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  int idx;
  for (idx=0; idx<num_ship; idx++)
    if (allShips[idx][0].ship.id == id)
      return Py_BuildValue("i", idx);
  return Py_BuildValue("i", -1);
}
static PyObject* py_enemyId(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx <= num_ship)
    return Py_BuildValue("i", allShips[idx][0].ship.id);
  else
    return Py_BuildValue("i", -1);
}
static PyObject* py_shipCountScreen(PyObject* pySelf, PyObject* args) {
  return Py_BuildValue("i", num_ship);
}
//Begin idx functions! -JNE
static PyObject* py_enemyDistance(PyObject* pySelf, PyObject* args) { //returns the distance of a ship with a particular index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (allShips[idx][0].d == 9999) {
    return Py_BuildValue("i", -1);
  }
  return Py_BuildValue("d",allShips[idx][0].d);
}
static PyObject* py_enemySpeed(PyObject* pySelf, PyObject* args) {  //returns velocity of a ship with a particular index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",allShips[idx][0].vel);
}
static PyObject* py_enemyVelX(PyObject* pySelf, PyObject* args) { //returns velocity of a ship with a particular index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",allShips[idx][0].velX);
}
static PyObject* py_enemyVelY(PyObject* pySelf, PyObject* args) { //returns velocity of a ship with a particular index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",allShips[idx][0].velY);
}
/*static PyObject* py_enemyReload(PyObject* pySelf, PyObject* args) { //returns velocity of a ship with a particular index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
  PyErr_SetString(PyExc_TypeError, "invalid parameter");
  return NULL;
  }
  return Py_BuildValue("i",allShips[idx][0].reload);
  }*/
double enemyTrackingRad() {  //returns the player's tracking in radians  -JNE  //DO NOT CHANGE, NEEDED IN ORDER FOR selfTrackingDeg to work -JRA
  if (vel.y == 0 && vel.x == 0) return selfHeadingRad(); //fix for NaN -EGG -CJG
  return atan2((double)vel.y,(double)vel.x);
}
static PyObject* py_enemyTrackingRad(PyObject* pySelf, PyObject* args) {  //returns tracking based on index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",allShips[idx][0].trackingRad);
}
static PyObject* py_enemyTrackingDeg(PyObject* pySelf, PyObject* args) {  //returns tracking based on index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",allShips[idx][0].trackingDeg);
}
static PyObject* py_enemyX(PyObject* pySelf, PyObject* args) {    //returns x coordinate of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("i",allShips[idx][0].ship.x);
}
static PyObject* py_enemyY(PyObject* pySelf, PyObject* args) {    //returns y coordinate of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("i",allShips[idx][0].ship.y);
}
static PyObject* py_enemyHeadingXdeg(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("i",allShips[idx][0].ship.dir);
}
static PyObject* py_enemyHeadingDeg(PyObject* pySelf, PyObject* args) {   //returns heading in degrees of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_xdegToDeg(allShips[idx][0].ship.dir));
}
static PyObject* py_enemyHeadingRad(PyObject* pySelf, PyObject* args) {   //returns heading in radians of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_xdegToRad(allShips[idx][0].ship.dir));
}
static PyObject* py_enemyShield(PyObject* pySelf, PyObject* args) {   //returns shield status of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("i",allShips[idx][0].ship.shield);
}
static PyObject* py_enemyLives(PyObject* pySelf, PyObject* args) {    //returns lives of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("i",Others[idx].life);
}
static PyObject* py_enemyTeam(PyObject* pySelf, PyObject* args) { //returns team of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  int i;
  for (i=0;i<num_others;i++) {
    if (Others[i].id == allShips[idx][0].ship.id) {
      return Py_BuildValue("i",Others[i].team);
    }
  }
  return Py_BuildValue("i",-1);
}
static PyObject* py_enemyName(PyObject* pySelf, PyObject* args) {   //returns name of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  int i;
  for (i=0;i<num_others;i++) {
    if (Others[i].id == allShips[idx][0].ship.id) {
      return Py_BuildValue("s",Others[i].name);
    }
  }
  return Py_BuildValue("s","");
}
static PyObject* py_enemyScore(PyObject* pySelf, PyObject* args) {    //returns score of enemy at an index -JNE
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  int i;
  for (i=0;i<num_others;i++) {
    if (Others[i].id == allShips[idx][0].ship.id) {
      return Py_BuildValue("d",Others[i].score);
    }
  }
  Py_RETURN_NONE;
}
//End idx functions. -JNE
static PyObject* py_xdegToDeg(PyObject* pySelf, PyObject* args) {
  double xdeg;
  if (!PyArg_ParseTuple(args, "d", &xdeg)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_xdegToDeg(xdeg));
}
static PyObject* py_xdegToRad(PyObject* pySelf, PyObject* args) {
  double xdeg;
  if (!PyArg_ParseTuple(args, "d", &xdeg)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_xdegToRad(xdeg));
}
static PyObject* py_degToXdeg(PyObject* pySelf, PyObject* args) {
  double deg;
  if (!PyArg_ParseTuple(args, "d", &deg)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_degToXdeg(deg));
}
static PyObject* py_degToRad(PyObject* pySelf, PyObject* args) {
  double deg;
  if (!PyArg_ParseTuple(args, "d", &deg)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_degToRad(deg));
}
static PyObject* py_radToXdeg(PyObject* pySelf, PyObject* args) {
  double rad;
  if (!PyArg_ParseTuple(args, "d", &rad)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_radToXdeg(rad));
}
//Converts radians (double) to degrees (double). -EGG -hatten
static PyObject* py_radToDeg(PyObject* pySelf, PyObject* args) {
  double rad;
  if (!PyArg_ParseTuple(args, "d", &rad)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  return Py_BuildValue("d",AI_radToDeg(rad));
}
//Returns the smallest angle which angle1 could add to itself to be equal to angle2. -EGG
static PyObject* py_angleDiffXdeg(PyObject* pySelf, PyObject* args) {
  double angle1, angle2;
  if (!PyArg_ParseTuple(args, "dd", &angle1, &angle2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double difference = angle2 - angle1;
  while (difference > 128)
    difference -= 128;
  while (difference < 128)
    difference += 128;
  return Py_BuildValue("d",fabs(difference));
}
static PyObject* py_angleDiffDeg(PyObject* pySelf, PyObject* args) {
  double angle1, angle2;
  if (!PyArg_ParseTuple(args, "dd", &angle1, &angle2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double difference = angle2 - angle1;
  while (difference > 360)
    difference -= 360;
  while (difference < -360)
    difference += 360;
  return Py_BuildValue("d",fabs(difference));
}
static PyObject* py_angleDiffRad(PyObject* pySelf, PyObject* args) {
  double angle1, angle2;
  if (!PyArg_ParseTuple(args, "dd", &angle1, &angle2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double difference = angle2 - angle1;
  while (difference > PI_AI)
    difference -= 2*PI_AI;
  while (difference < -PI_AI)
    difference += 2*PI_AI;
  return Py_BuildValue("d",fabs(difference));
}
//Returns the result of adding two angles together. -EGG
static PyObject* py_angleAddXdeg(PyObject* pySelf, PyObject* args) {
  double angle1, angle2;
  if (!PyArg_ParseTuple(args, "dd", &angle1, &angle2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double sum = angle1+angle2;
  while (sum > 64)
    sum -= 128;
  while (sum < -64)
    sum += 128;
  return Py_BuildValue("d",sum);
}
static PyObject* py_angleAddDeg(PyObject* pySelf, PyObject* args) {
  double angle1, angle2;
  if (!PyArg_ParseTuple(args, "dd", &angle1, &angle2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double sum = angle1+angle2;
  while (sum > 180)
    sum -= 360;
  while (sum < -180)
    sum += 360;
  return Py_BuildValue("d",sum);
}
static PyObject* py_angleAddRad(PyObject* pySelf, PyObject* args) {
  double angle1, angle2;
  if (!PyArg_ParseTuple(args, "dd", &angle1, &angle2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double sum = angle1+angle2;
  while (sum > PI_AI)
    sum -= 2*PI_AI;
  while (sum < -PI_AI)
    sum += 2*PI_AI;
  return Py_BuildValue("d",sum);
}
//wall_here -EGG
//Parameters: x, y, flag to draw wall feelers, flag to draw wall detection. -EGG
//Returns 1 if there is a wall at the given x,y or 0 if not. -EGG
//Removed detection drawing flags -CJG
int wall_here(int x, int y) { //DO NOT CHANGE -JRA
  int     index,type;

  //Convert from pixel- to block-coordinates
  x = x / BLOCK_SZ;
  y = y / BLOCK_SZ;

  //If we don't have wrap play the end of the map are walls.
  if (!BIT(Setup->mode, WRAP_PLAY)) {
    if (x < 0 || y < 0 || x >= Setup->x || y >= Setup->y)
      return 1;
  }

  //Modulus so we're on the map
  x = x % Setup->x;
  y = y % Setup->y;

  //Modulus returns negative values for negative divisors
  if (x < 0) {
    x += Setup->x;
  }
  if (y < 0) {
    y += Setup->y;
  }

  //calculate index in the map_data array
  index = y + Setup->y * x;

  //look up what the type is
  type=Setup->map_data[index];

  if (type & BLUE_BIT)
    return 1; //TODO: Check for triangles
  return 0;
}
//wallFeeler! -EGG
//Parameters: distance of line to 'feel', angle in degrees, flag to draw wall feelers, flag to draw wall detection. -EGG
//Returns 1 if there is a wall from the player's ship at the given angle and distance or 0 if not. -EGG
static PyObject* py_wallFeelerDeg(PyObject* pySelf, PyObject* args) { //removed flags -CJG
  double dist, angle, a, x, y, ret;
  if (!PyArg_ParseTuple(args, "dd", &dist, &angle)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  a = AI_degToRad(angle);
  x = pos.x + cos(a)*dist;
  y = pos.y + sin(a)*dist;
  ret = wallBetween((double)pos.x, (double)pos.y, x, y);
  //if (ret == -1) return Py_BuildValue("i",dist); //Returns the distance of the feeler if no wall is felt - JTO
  return Py_BuildValue("d",ret);
}
//wallFeeler that uses radians! -EGG
static PyObject* py_wallFeelerRad(PyObject* pySelf, PyObject* args) { //Removed flags -CJG
  double dist, a, x, y, ret; 
  if (!PyArg_ParseTuple(args, "dd", &dist, &a)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  x = pos.x + cos(a)*dist;
  y = pos.y + sin(a)*dist;
  ret =  wallBetween((double)pos.x, (double)pos.y, x, y);
  //if (ret == -1) return  Py_BuildValue("i",dist); //Returns the distance of the feeler if no wall is felt - JTO
  return Py_BuildValue("d",ret);
}
//Map option functions --hatten
int blockSize(void) {
  return BLOCK_SZ;
}
int mapWidthBlocks(void) {
  return Setup->x;
}
int mapHeightBlocks(void) {
  return Setup->y;
}
int mapWidthPixels(void) {
  return Setup->width;
}
int mapHeightPixels(void) {
  return Setup->height;
}
int Find_closest_team(posx,posy) {
  int i,x,y,team;
  double dist, best=-1;
  for (i=0; i<num_bases;i++) {
    x = bases[i].pos / Setup->y;
    y = bases[i].pos % Setup->y;
    if (BIT(Setup->mode, WRAP_PLAY)) {
      wrapWhole(posx, posy, &x, &y, Setup->x, Setup->y, &dist);
    }
    if (dist < best || best == -1) {
      best=dist;
      team=bases[i].team;
    }
  }
  return team;
}

static PyObject* py_mapData(PyObject* pySelf, PyObject* args) {
  int x,y,index,result;
  if (!PyArg_ParseTuple(args, "ii", &x, &y)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  index=y + Setup->y * x;
  if (index < Setup->y * Setup->x && y < Setup->y && x < Setup->x) {
    result=Setup->map_data[index];
    if (result == SETUP_SPACE_DOT) //19
      result=SETUP_SPACE; //0
    else if (result & BLUE_BIT) {
      if (result & BLUE_FUEL == BLUE_FUEL) {
        result=SETUP_FUEL; //3
      }
      else if (result & BLUE_OPEN) {
        if (result & BLUE_BELOW) {
          result=SETUP_REC_RD; //5
        }
        else {
          result=SETUP_REC_LU; //6
        }
      }
      else if (result & BLUE_CLOSED) {
        if (result & BLUE_BELOW) {
          result=SETUP_REC_LD; //7
        }
        else {
          result=SETUP_REC_RU; //4
        }
      }
      else {
        result=SETUP_FILLED; //1
      }

    }
    else if (result == SETUP_TREASURE) {
      if (BIT(Setup->mode, TEAM_PLAY))
        result += Find_closest_team(x,y);
    }
    else if (result == SETUP_BASE_UP || //30
        result == SETUP_BASE_RIGHT || //40
        result == SETUP_BASE_DOWN || //50
        result == SETUP_BASE_LEFT) { //60 
      int id,team;
      Base_info_by_pos(x,y,&id,&team);
      result += team;
    }
    else if (result == SETUP_TARGET) //70
      if (BIT(Setup->mode, TEAM_PLAY))
        result += Find_closest_team(x,y);
      else if (result == SETUP_CHECK) //80
        result += Check_index_by_pos(x, y);
    return Py_BuildValue("i", result);
  }
  return Py_BuildValue("i",-1);
}
void fillOptions() {
  int i;
  for (i=0; i < storedOptionCount; i++) {
    storedOptions[i].status = UNSET;
  }
  strlcpy(storedOptions[0].name, "mapdata", 64);
  storedOptions[1].status = UNAVAILABLE;
  strlcpy(storedOptions[1].name, "mapwidth", 64);
  storedOptions[1].status = SET;
  storedOptions[1].type = INTTYPE;
  storedOptions[1].intValue = Setup->x;
  strlcpy(storedOptions[2].name, "mapheight", 64);
  storedOptions[2].status = SET;
  storedOptions[2].type = INTTYPE;
  storedOptions[2].intValue = Setup->y;
  strlcpy(storedOptions[3].name, "mapname", 64);
  storedOptions[3].status = SET;
  storedOptions[3].type = STRINGTYPE;
  strlcpy(storedOptions[3].stringValue, Setup->name, 64);
  strlcpy(storedOptions[4].name, "mapauthor", 64);
  storedOptions[4].status = SET;
  storedOptions[4].type = STRINGTYPE;
  strlcpy(storedOptions[4].stringValue, Setup->author, 64);
  strlcpy(storedOptions[5].name, "limitedlives", 64);
  storedOptions[5].status = SET;
  storedOptions[5].type = BOOLTYPE;
  storedOptions[5].intValue = BIT(Setup->mode, LIMITED_LIVES)?1:0;
  strlcpy(storedOptions[6].name, "worldlives", 64);
  storedOptions[6].status = SET;
  storedOptions[6].type = BOOLTYPE;
  storedOptions[6].intValue = Setup->lives;
  strlcpy(storedOptions[7].name, "selfimmunity", 64);
  storedOptions[7].type = BOOLTYPE;
  strlcpy(storedOptions[8].name, "gravity", 64);
  storedOptions[8].type = DOUBLETYPE;
  strlcpy(storedOptions[9].name, "gravityangle", 64);
  storedOptions[8].type = DOUBLETYPE;
  strlcpy(storedOptions[10].name, "gravitypoint", 64);
  storedOptions[10].status = UNAVAILABLE;
  strlcpy(storedOptions[11].name, "gravitypointsource", 64);
  storedOptions[11].type = BOOLTYPE;
  strlcpy(storedOptions[12].name, "gravityclockwise", 64);
  storedOptions[12].type = BOOLTYPE;
  strlcpy(storedOptions[13].name, "gravityanticlockwise", 64);
  storedOptions[13].type = BOOLTYPE;
  strlcpy(storedOptions[14].name, "shotsgravity", 64);
  storedOptions[14].type = BOOLTYPE;
  strlcpy(storedOptions[15].name, "gravityvisible", 64);
  storedOptions[15].type = BOOLTYPE;
  strlcpy(storedOptions[16].name, "coriolis", 64);
  storedOptions[16].type = DOUBLETYPE;
  strlcpy(storedOptions[17].name, "friction", 64);
  storedOptions[17].type = DOUBLETYPE;
  strlcpy(storedOptions[18].name, "blockfriction", 64);
  storedOptions[18].type = DOUBLETYPE;
  strlcpy(storedOptions[19].name, "defaultshipshape", 64);
  storedOptions[19].status = UNAVAILABLE;
  strlcpy(storedOptions[20].name, "tankshipshape", 64);
  storedOptions[20].status = UNAVAILABLE;
  strlcpy(storedOptions[21].name, "shipmass", 64);
  storedOptions[21].type = DOUBLETYPE;
  strlcpy(storedOptions[22].name, "shotmass", 64);
  storedOptions[22].type = DOUBLETYPE;
  strlcpy(storedOptions[23].name, "shotspeed", 64);
  storedOptions[23].type = DOUBLETYPE;
  strlcpy(storedOptions[24].name, "shotlife", 64);
  storedOptions[24].type = INTTYPE;
  strlcpy(storedOptions[25].name, "maxplayershots", 64);
  storedOptions[25].type = INTTYPE;
  strlcpy(storedOptions[26].name, "firerepeatrate", 64);
  storedOptions[26].type = INTTYPE;
  strlcpy(storedOptions[27].name, "keepshots", 64);
  storedOptions[27].type = BOOLTYPE;
  strlcpy(storedOptions[28].name, "edgebounce", 64);
  storedOptions[28].type = BOOLTYPE;
  strlcpy(storedOptions[29].name, "edgewrap", 64);
  storedOptions[29].status = SET;
  storedOptions[29].type = BOOLTYPE;
  storedOptions[29].intValue = BIT(Setup->mode, WRAP_PLAY)?1:0;
  strlcpy(storedOptions[30].name, "extraborder", 64);
  storedOptions[30].type = BOOLTYPE;
  strlcpy(storedOptions[31].name, "turnthrust", 64);
  storedOptions[31].type = BOOLTYPE;
  strlcpy(storedOptions[32].name, "robotstalk", 64);
  storedOptions[32].type = BOOLTYPE;
  strlcpy(storedOptions[33].name, "robotsleave", 64);
  storedOptions[33].type = BOOLTYPE;
  strlcpy(storedOptions[34].name, "robotleavelife", 64);
  storedOptions[34].type = INTTYPE;
  strlcpy(storedOptions[35].name, "robotleavescore", 64);
  storedOptions[35].type = INTTYPE;
  strlcpy(storedOptions[36].name, "robotleaveratio", 64);
  storedOptions[36].type = INTTYPE;
  strlcpy(storedOptions[37].name, "robotteam", 64);
  storedOptions[37].type = INTTYPE;
  strlcpy(storedOptions[38].name, "restrictrobots", 64);
  storedOptions[38].type = BOOLTYPE;
  strlcpy(storedOptions[39].name, "reserverobotteam", 64);
  storedOptions[39].type = BOOLTYPE;
  strlcpy(storedOptions[40].name, "minrobots", 64);
  storedOptions[40].type = INTTYPE;
  strlcpy(storedOptions[41].name, "maxrobots", 64);
  storedOptions[41].type = INTTYPE;
  strlcpy(storedOptions[42].name, "robotrealname", 64);
  storedOptions[42].type = STRINGTYPE;
  strlcpy(storedOptions[43].name, "robothostname", 64);
  storedOptions[43].type = STRINGTYPE;
  strlcpy(storedOptions[44].name, "shotswallbounce", 64);
  storedOptions[44].type = BOOLTYPE;
  strlcpy(storedOptions[45].name, "ballswallbounce", 64);
  storedOptions[45].type = BOOLTYPE;
  strlcpy(storedOptions[46].name, "mineswallbounce", 64);
  storedOptions[46].type = BOOLTYPE;
  strlcpy(storedOptions[47].name, "itemswallbounce", 64);
  storedOptions[47].type = BOOLTYPE;
  strlcpy(storedOptions[48].name, "missileswallbounce", 64);
  storedOptions[48].type = BOOLTYPE;
  strlcpy(storedOptions[49].name, "sparkswallbounce", 64);
  storedOptions[49].type = BOOLTYPE;
  strlcpy(storedOptions[50].name, "debriswallbounce", 64);
  storedOptions[50].type = BOOLTYPE;
  strlcpy(storedOptions[51].name, "asteroidswallbounce", 64);
  storedOptions[51].type = BOOLTYPE;
  strlcpy(storedOptions[52].name, "wreckagecollisionmaykill", 64);
  storedOptions[52].type = BOOLTYPE;
  strlcpy(storedOptions[53].name, "tankrealname", 64);
  storedOptions[53].type = STRINGTYPE;
  strlcpy(storedOptions[54].name, "tankhostname", 64);
  storedOptions[54].type = STRINGTYPE;
  strlcpy(storedOptions[55].name, "maxobjectwallbouncespeed", 64);
  storedOptions[55].type = DOUBLETYPE;
  strlcpy(storedOptions[56].name, "maxshieldedwallbouncespeed", 64);
  storedOptions[56].type = DOUBLETYPE;
  strlcpy(storedOptions[57].name, "maxunshieldedwallbouncespeed", 64);
  storedOptions[57].type = DOUBLETYPE;
  strlcpy(storedOptions[58].name, "maxshieldedplayerwallbounceangle", 64);
  storedOptions[58].type = DOUBLETYPE;
  strlcpy(storedOptions[59].name, "maxunshieldedplayerwallbounceangle", 64);
  storedOptions[59].type = DOUBLETYPE;
  strlcpy(storedOptions[60].name, "playerwallbouncebrakefactor", 64);
  storedOptions[60].type = DOUBLETYPE;
  strlcpy(storedOptions[61].name, "objectwallbouncebrakefactor", 64);
  storedOptions[61].type = DOUBLETYPE;
  strlcpy(storedOptions[62].name, "objectwallbouncelifefactor", 64);
  storedOptions[62].type = DOUBLETYPE;
  strlcpy(storedOptions[63].name, "wallbouncefueldrainmult", 64);
  storedOptions[63].type = DOUBLETYPE;
  strlcpy(storedOptions[64].name, "wallbouncedestroyitemprob", 64);
  storedOptions[64].type = DOUBLETYPE;
  strlcpy(storedOptions[65].name, "loseitemdestroys", 64);
  storedOptions[65].type = BOOLTYPE;
  strlcpy(storedOptions[66].name, "limitedvisibility", 64);
  storedOptions[66].status = SET;
  storedOptions[66].type = BOOLTYPE;
  storedOptions[66].intValue = BIT(Setup->mode, LIMITED_VISIBILITY)?1:0;
  strlcpy(storedOptions[67].name, "minvisibilitydistance", 64);
  storedOptions[67].type = DOUBLETYPE;
  strlcpy(storedOptions[68].name, "maxvisibilitydistance", 64);
  storedOptions[68].type = DOUBLETYPE;
  strlcpy(storedOptions[69].name, "wormholevisible", 64);
  storedOptions[69].type = BOOLTYPE;
  strlcpy(storedOptions[70].name, "itemconcentratorvisible", 64);
  storedOptions[70].type = BOOLTYPE;
  strlcpy(storedOptions[71].name, "blockfrictionvisible", 64);
  storedOptions[71].type = BOOLTYPE;
  strlcpy(storedOptions[72].name, "wormtime", 64);
  storedOptions[72].type = INTTYPE;
  strlcpy(storedOptions[73].name, "playerstartsshielded", 64);
  storedOptions[73].type = BOOLTYPE;
  strlcpy(storedOptions[74].name, "shieldeditempickup", 64);
  storedOptions[74].type = BOOLTYPE;
  strlcpy(storedOptions[75].name, "shieldedmining", 64);
  storedOptions[75].type = BOOLTYPE;
  strlcpy(storedOptions[76].name, "allowalliances", 64);
  storedOptions[76].status = SET;
  storedOptions[76].type = BOOLTYPE;
  storedOptions[76].intValue = BIT(Setup->mode, ALLIANCES)?1:0;
  strlcpy(storedOptions[77].name, "announcealliances", 64);
  storedOptions[77].type = BOOLTYPE;
  strlcpy(storedOptions[78].name, "targetkillteam", 64);
  storedOptions[78].type = BOOLTYPE;
  strlcpy(storedOptions[79].name, "targetteamcollision", 64);
  storedOptions[79].type = BOOLTYPE;
  strlcpy(storedOptions[80].name, "targetsync", 64);
  storedOptions[80].type = BOOLTYPE;
  strlcpy(storedOptions[81].name, "targetdeadtime", 64);
  storedOptions[81].type = INTTYPE;
  strlcpy(storedOptions[82].name, "treasurekillteam", 64);
  storedOptions[82].type = BOOLTYPE;
  strlcpy(storedOptions[83].name, "treasurecollisiondestroys", 64);
  storedOptions[83].type = BOOLTYPE;
  strlcpy(storedOptions[84].name, "treasurecollisionmaykill", 64);
  storedOptions[84].type = BOOLTYPE;
  strlcpy(storedOptions[85].name, "ballconnectorlength", 64);
  storedOptions[85].type = DOUBLETYPE;
  strlcpy(storedOptions[86].name, "maxballconnectorratio", 64);
  storedOptions[86].type = DOUBLETYPE;
  strlcpy(storedOptions[87].name, "ballconnectordamping", 64);
  storedOptions[87].type = DOUBLETYPE;
  strlcpy(storedOptions[88].name, "ballconnectorspringconstant", 64);
  storedOptions[88].type = DOUBLETYPE;
  strlcpy(storedOptions[89].name, "connectorisstring", 64);
  storedOptions[89].type = BOOLTYPE;
  strlcpy(storedOptions[90].name, "ballcollisions", 64);
  storedOptions[90].type = BOOLTYPE;
  strlcpy(storedOptions[91].name, "ballsparkcollisions", 64);
  storedOptions[91].type = BOOLTYPE;
  strlcpy(storedOptions[92].name, "ballmass", 64);
  storedOptions[92].type = DOUBLETYPE;
  strlcpy(storedOptions[93].name, "playersonradar", 64);
  storedOptions[93].type = BOOLTYPE;
  strlcpy(storedOptions[94].name, "missilesonradar", 64);
  storedOptions[94].type = BOOLTYPE;
  strlcpy(storedOptions[95].name, "minesonradar", 64);
  storedOptions[95].type = BOOLTYPE;
  strlcpy(storedOptions[96].name, "nukesonradar", 64);
  storedOptions[96].type = BOOLTYPE;
  strlcpy(storedOptions[97].name, "treasuresonradar", 64);
  storedOptions[97].type = BOOLTYPE;
  strlcpy(storedOptions[98].name, "teamplay", 64);
  storedOptions[98].status = SET;
  storedOptions[98].type = BOOLTYPE;
  storedOptions[98].intValue = BIT(Setup->mode, TEAM_PLAY)?1:0;
  strlcpy(storedOptions[99].name, "teamassign", 64);
  storedOptions[99].type = BOOLTYPE;
  strlcpy(storedOptions[100].name, "teamimmunity", 64);
  storedOptions[100].type = BOOLTYPE;
  strlcpy(storedOptions[101].name, "teamcannons", 65);
  storedOptions[101].type = BOOLTYPE;
  strlcpy(storedOptions[102].name, "teamfuel", 64);
  storedOptions[102].type = BOOLTYPE;
  strlcpy(storedOptions[103].name, "capturetheflag", 64);
  storedOptions[103].type = BOOLTYPE;
  strlcpy(storedOptions[104].name, "cloakedexhaust", 64);
  storedOptions[104].type = BOOLTYPE;
  strlcpy(storedOptions[105].name, "cloakedshield", 64);
  storedOptions[105].type = BOOLTYPE;
  strlcpy(storedOptions[106].name, "cannonsuseitems", 64);
  storedOptions[106].type = BOOLTYPE;
  strlcpy(storedOptions[107].name, "cannonsdefend", 64);
  storedOptions[107].type = BOOLTYPE;
  strlcpy(storedOptions[108].name, "cannonsmartness", 64);
  storedOptions[108].type = INTTYPE;
  strlcpy(storedOptions[109].name, "cannondeadtime", 64);
  storedOptions[109].type = INTTYPE;
  strlcpy(storedOptions[110].name, "cannonflak", 64);
  storedOptions[110].type = BOOLTYPE;
  strlcpy(storedOptions[111].name, "identifymines", 64);
  storedOptions[111].type = BOOLTYPE;
  strlcpy(storedOptions[112].name, "maxminesperpack", 64);
  storedOptions[112].type = INTTYPE;
  strlcpy(storedOptions[113].name, "minelife", 64);
  storedOptions[113].type = INTTYPE;
  strlcpy(storedOptions[114].name, "minefusetime", 64);
  storedOptions[114].type = DOUBLETYPE;
  strlcpy(storedOptions[115].name, "baseminerange", 64);
  storedOptions[115].type = INTTYPE;
  strlcpy(storedOptions[116].name, "mineshotdetonatedistance", 64);
  storedOptions[116].type = INTTYPE;
  strlcpy(storedOptions[117].name, "roguemineprob", 64);
  storedOptions[117].type = DOUBLETYPE;
  strlcpy(storedOptions[118].name, "nukeminmines", 64);
  storedOptions[118].type = INTTYPE;
  strlcpy(storedOptions[119].name, "minminespeed", 64);
  storedOptions[119].type = INTTYPE;
  strlcpy(storedOptions[120].name, "nukeclusterdamage", 64);
  storedOptions[120].type = DOUBLETYPE;
  strlcpy(storedOptions[121].name, "ecmsreprogramrobots", 64);
  storedOptions[121].type = BOOLTYPE;
  strlcpy(storedOptions[122].name, "ecmsreprogrammines", 64);
  storedOptions[122].type = BOOLTYPE;
  strlcpy(storedOptions[123].name, "distinguishmissiles", 64);
  storedOptions[123].type = BOOLTYPE;
  strlcpy(storedOptions[124].name, "maxmissilesperpack", 64);
  storedOptions[124].type = INTTYPE;
  strlcpy(storedOptions[125].name, "missilelife", 64);
  storedOptions[125].type = INTTYPE;
  strlcpy(storedOptions[126].name, "rogueheatprob", 64);
  storedOptions[126].type = DOUBLETYPE;
  strlcpy(storedOptions[127].name, "nukeminsmarts", 64);
  storedOptions[127].type = INTTYPE;
  strlcpy(storedOptions[128].name, "asteroidcollisionmaykill", 64);
  storedOptions[128].type = BOOLTYPE;
  strlcpy(storedOptions[129].name, "asteroidsonradar", 64);
  storedOptions[129].type = BOOLTYPE;
  strlcpy(storedOptions[130].name, "maxasteroiddensity", 64);
  storedOptions[130].type = DOUBLETYPE;
  strlcpy(storedOptions[131].name, "asteroidconcentratorvisible", 64);
  storedOptions[131].type = BOOLTYPE;
  strlcpy(storedOptions[132].name, "asteroidconcentratorradius", 64);
  storedOptions[132].type = INTTYPE;
  strlcpy(storedOptions[133].name, "asteroidmaxitems", 64);
  storedOptions[133].type = DOUBLETYPE;
  strlcpy(storedOptions[134].name, "allowshipshapes", 64);
  storedOptions[134].type = BOOLTYPE;
  strlcpy(storedOptions[135].name, "allowsmartmissiles", 64);
  storedOptions[135].type = BOOLTYPE;
  strlcpy(storedOptions[136].name, "allowheatseekers", 64);
  storedOptions[136].type = BOOLTYPE;
  strlcpy(storedOptions[137].name, "allowtorpedoes", 64);
  storedOptions[137].type = BOOLTYPE;
  strlcpy(storedOptions[138].name, "allowplayercrashes", 64);
  storedOptions[138].status = SET;
  storedOptions[138].type = BOOLTYPE;
  storedOptions[138].intValue = BIT(Setup->mode, CRASH_WITH_PLAYER)?1:0;
  strlcpy(storedOptions[139].name, "allowplayerbounces", 64);
  storedOptions[139].status = SET;
  storedOptions[139].type = BOOLTYPE;
  storedOptions[139].intValue = BIT(Setup->mode, BOUNCE_WITH_PLAYER)?1:0;
  strlcpy(storedOptions[140].name, "allowplayerkilling", 64);
  storedOptions[140].status = SET;
  storedOptions[140].type = BOOLTYPE;
  storedOptions[140].intValue = BIT(Setup->mode, PLAYER_KILLINGS)?1:0;
  strlcpy(storedOptions[141].name, "allowshields", 64);
  storedOptions[141].status = SET;
  storedOptions[141].type = BOOLTYPE;
  storedOptions[141].intValue = BIT(Setup->mode, PLAYER_SHIELDING)?1:0;
  strlcpy(storedOptions[142].name, "allownukes", 64);
  storedOptions[142].status = SET;
  storedOptions[142].type = BOOLTYPE;
  storedOptions[142].intValue = BIT(Setup->mode, ALLOW_NUKES)?1:0;
  strlcpy(storedOptions[143].name, "allowclusters", 64);
  storedOptions[143].status = SET;
  storedOptions[143].type = BOOLTYPE;
  storedOptions[143].intValue = BIT(Setup->mode, ALLOW_CLUSTERS)?1:0;
  strlcpy(storedOptions[144].name, "allowmodifiers", 64);
  storedOptions[144].status = SET;
  storedOptions[144].type = BOOLTYPE;
  storedOptions[144].intValue = BIT(Setup->mode, ALLOW_MODIFIERS)?1:0;
  strlcpy(storedOptions[145].name, "allowlasermodifiers", 64);
  storedOptions[145].status = SET;
  storedOptions[145].type = BOOLTYPE;
  storedOptions[145].intValue = BIT(Setup->mode, ALLOW_LASER_MODIFIERS)?1:0;
  strlcpy(storedOptions[146].name, "laserisstungun", 64);
  storedOptions[146].type = BOOLTYPE;
  strlcpy(storedOptions[147].name, "maxroundtime", 64);
  storedOptions[147].type = INTTYPE;
  strlcpy(storedOptions[148].name, "gameduration", 64);
  storedOptions[148].type = DOUBLETYPE;
  strlcpy(storedOptions[149].name, "rounddelay", 64);
  storedOptions[149].type = INTTYPE;
  strlcpy(storedOptions[150].name, "roundstoplay", 64);
  storedOptions[150].type = INTTYPE;
  strlcpy(storedOptions[151].name, "reset", 64);
  storedOptions[151].type = BOOLTYPE;
  strlcpy(storedOptions[152].name, "resetonhuman", 64);
  storedOptions[152].type = INTTYPE;
  strlcpy(storedOptions[153].name, "maxpausetime", 64);
  storedOptions[153].type = INTTYPE;
  strlcpy(storedOptions[154].name, "timing", 64);
  storedOptions[154].status = SET;
  storedOptions[154].type = BOOLTYPE;
  storedOptions[154].intValue = BIT(Setup->mode, TIMING)?1:0;
  strlcpy(storedOptions[155].name, "checkpointradius", 64);
  storedOptions[155].type = DOUBLETYPE;
  strlcpy(storedOptions[156].name, "racelaps", 64);
  storedOptions[156].type = INTTYPE;
  strlcpy(storedOptions[157].name, "ballrace", 64);
  storedOptions[157].type = BOOLTYPE;
  strlcpy(storedOptions[158].name, "ballraceconnected", 64);
  storedOptions[158].type = BOOLTYPE;
  strlcpy(storedOptions[159].name, "itemconcentratorradius", 64);
  storedOptions[159].type = INTTYPE;
  strlcpy(storedOptions[160].name, "usewreckage", 64);
  storedOptions[160].type = BOOLTYPE;
  strlcpy(storedOptions[161].name, "lockotherteam", 64);
  storedOptions[161].type = BOOLTYPE;
  strlcpy(storedOptions[162].name, "allowviewing", 64);
  storedOptions[162].type = BOOLTYPE;
  strlcpy(storedOptions[163].name, "framespersecond", 64);
  storedOptions[163].status = SET;
  storedOptions[163].type = BOOLTYPE;
  storedOptions[163 ].intValue = Setup->frames_per_second;
  strlcpy(storedOptions[164].name, "initialfuel", 64);
  storedOptions[164].type = INTTYPE;
  strlcpy(storedOptions[165].name, "initialtanks", 64);
  storedOptions[165].type = INTTYPE;
  strlcpy(storedOptions[166].name, "initialecms", 64);
  storedOptions[166].type = INTTYPE;
  strlcpy(storedOptions[167].name, "initialmines", 64);
  storedOptions[167].type = INTTYPE;
  strlcpy(storedOptions[168].name, "initialmissiles", 64);
  storedOptions[168].type = INTTYPE;
  strlcpy(storedOptions[169].name, "initialcloaks", 64);
  storedOptions[169].type = INTTYPE;
  strlcpy(storedOptions[170].name, "initialsensors", 64);
  storedOptions[170].type = INTTYPE;
  strlcpy(storedOptions[171].name, "initialwideangles", 64);
  storedOptions[171].type = INTTYPE;
  strlcpy(storedOptions[172].name, "initialrearshots", 64);
  storedOptions[172].type = INTTYPE;
  strlcpy(storedOptions[173].name, "initialafterburners", 64);
  storedOptions[173].type = INTTYPE;
  strlcpy(storedOptions[174].name, "initialtransporters", 64);
  storedOptions[174].type = INTTYPE;
  strlcpy(storedOptions[175].name, "initialdeflectors", 64);
  storedOptions[175].type = INTTYPE;
  strlcpy(storedOptions[176].name, "initialphasings", 64);
  storedOptions[176].type = INTTYPE;
  strlcpy(storedOptions[177].name, "initialhyperjumps", 64);
  storedOptions[177].type = INTTYPE;
  strlcpy(storedOptions[178].name, "initialemergencythrusts", 64);
  storedOptions[178].type = INTTYPE;
  strlcpy(storedOptions[179].name, "initiallasers", 64);
  storedOptions[179].type = INTTYPE;
  strlcpy(storedOptions[180].name, "initialtractorbeams", 64);
  storedOptions[180].type = INTTYPE;
  strlcpy(storedOptions[181].name, "initialautopilots", 64);
  storedOptions[181].type = INTTYPE;
  strlcpy(storedOptions[182].name, "initialemergencyshields", 64);
  storedOptions[182].type = INTTYPE;
  strlcpy(storedOptions[183].name, "initialmirrors", 64);
  storedOptions[183].type = INTTYPE;
  strlcpy(storedOptions[184].name, "initialarmor", 64);
  storedOptions[184].type = INTTYPE;
  strlcpy(storedOptions[185].name, "maxfuel", 64);
  storedOptions[185].type = INTTYPE;
  strlcpy(storedOptions[186].name, "maxtanks", 64);
  storedOptions[186].type = INTTYPE;
  strlcpy(storedOptions[187].name, "maxecms", 64);
  storedOptions[187].type = INTTYPE;
  strlcpy(storedOptions[188].name, "maxmines", 64);
  storedOptions[188].type = INTTYPE;
  strlcpy(storedOptions[189].name, "maxmissiles", 64);
  storedOptions[189].type = INTTYPE;
  strlcpy(storedOptions[190].name, "maxcloaks", 64);
  storedOptions[190].type = INTTYPE;
  strlcpy(storedOptions[191].name, "maxsensors", 64);
  storedOptions[191].type = INTTYPE;
  strlcpy(storedOptions[192].name, "maxwideangles", 64);
  storedOptions[192].type = INTTYPE;
  strlcpy(storedOptions[193].name, "maxrearshots", 64);
  storedOptions[193].type = INTTYPE;
  strlcpy(storedOptions[194].name, "maxafterburners", 64);
  storedOptions[194].type = INTTYPE;
  strlcpy(storedOptions[195].name, "maxtransporters", 64);
  storedOptions[195].type = INTTYPE;
  strlcpy(storedOptions[196].name, "maxdeflectors", 64);
  storedOptions[196].type = INTTYPE;
  strlcpy(storedOptions[197].name, "maxphasings", 64);
  storedOptions[197].type = INTTYPE;
  strlcpy(storedOptions[198].name, "maxhyperjumps", 64);
  storedOptions[198].type = INTTYPE;
  strlcpy(storedOptions[199].name, "maxemergencythrusts", 64);
  storedOptions[199].type = INTTYPE;
  strlcpy(storedOptions[200].name, "maxlasers", 64);
  storedOptions[200].type = INTTYPE;
  strlcpy(storedOptions[201].name, "maxtractorbeams", 64);
  storedOptions[201].type = INTTYPE;
  strlcpy(storedOptions[202].name, "maxautopilots", 64);
  storedOptions[202].type = INTTYPE;
  strlcpy(storedOptions[203].name, "maxemergencyshields", 64);
  storedOptions[203].type = INTTYPE;
  strlcpy(storedOptions[204].name, "maxmirrors", 64);
  storedOptions[204].type = INTTYPE;
  strlcpy(storedOptions[205].name, "maxarmor", 64);
  storedOptions[205].type = INTTYPE;
  strlcpy(storedOptions[206].name, "maxoffensiveitems", 64);
  storedOptions[206].type = INTTYPE;
  strlcpy(storedOptions[207].name, "maxdefensiveitems", 64);
  storedOptions[207].type = INTTYPE;
  strlcpy(storedOptions[208].name, "maxitemdensity", 64);
  storedOptions[208].type = DOUBLETYPE;
  strlcpy(storedOptions[209].name, "itemenergypackprob", 64);
  storedOptions[209].type = DOUBLETYPE;
  strlcpy(storedOptions[210].name, "itemtankprob", 64);
  storedOptions[210].type = DOUBLETYPE;
  strlcpy(storedOptions[211].name, "itemecmprob", 64);
  storedOptions[211].type = DOUBLETYPE;
  strlcpy(storedOptions[212].name, "itemmineprob", 64);
  storedOptions[212].type = DOUBLETYPE;
  strlcpy(storedOptions[213].name, "itemmissileprob", 64);
  storedOptions[213].type = DOUBLETYPE;
  strlcpy(storedOptions[214].name, "itemcloakprob", 64);
  storedOptions[214].type = DOUBLETYPE;
  strlcpy(storedOptions[215].name, "itemsensorprob", 64);
  storedOptions[215].type = DOUBLETYPE;
  strlcpy(storedOptions[216].name, "itemwideangleprob", 64);
  storedOptions[216].type = DOUBLETYPE;
  strlcpy(storedOptions[217].name, "itemrearshotprob", 64);
  storedOptions[217].type = DOUBLETYPE;
  strlcpy(storedOptions[218].name, "itemafterburnerprob", 64);
  storedOptions[218].type = DOUBLETYPE;
  strlcpy(storedOptions[219].name, "itemtransporterprob", 64);
  storedOptions[219].type = DOUBLETYPE;
  strlcpy(storedOptions[220].name, "itemlaserprob", 64);
  storedOptions[220].type = DOUBLETYPE;
  strlcpy(storedOptions[221].name, "itememergencythrustprob", 64);
  storedOptions[221].type = DOUBLETYPE;
  strlcpy(storedOptions[222].name, "itemtractorbeamprob", 64);
  storedOptions[222].type = DOUBLETYPE;
  strlcpy(storedOptions[223].name, "itemautopilotprob", 64);
  storedOptions[223].type = DOUBLETYPE;
  strlcpy(storedOptions[224].name, "itememergencyshieldprob", 64);
  storedOptions[224].type = DOUBLETYPE;
  strlcpy(storedOptions[225].name, "itemdeflectorprob", 64);
  storedOptions[225].type = DOUBLETYPE;
  strlcpy(storedOptions[226].name, "itemhyperjumpprob", 64);
  storedOptions[226].type = DOUBLETYPE;
  strlcpy(storedOptions[227].name, "itemphasingprob", 64);
  storedOptions[227].type = DOUBLETYPE;
  strlcpy(storedOptions[228].name, "itemmirrorprob", 64);
  storedOptions[228].type = DOUBLETYPE;
  strlcpy(storedOptions[229].name, "itemarmorprob", 64);
  storedOptions[229].type = DOUBLETYPE;
  strlcpy(storedOptions[230].name, "movingitemprob", 64);
  storedOptions[230].type = DOUBLETYPE;
  strlcpy(storedOptions[231].name, "dropitemonkillprob", 64);
  storedOptions[231].type = DOUBLETYPE;
  strlcpy(storedOptions[232].name, "detonateitemonkillprob", 64);
  storedOptions[232].type = DOUBLETYPE;
  strlcpy(storedOptions[233].name, "destroyitemincollisionprob", 64);
  storedOptions[233].type = DOUBLETYPE;
  strlcpy(storedOptions[234].name, "itemconcentratorprob", 64);
  storedOptions[234].type = DOUBLETYPE;
  strlcpy(storedOptions[235].name, "itemprobmult", 64);
  storedOptions[235].type = DOUBLETYPE;
  strlcpy(storedOptions[236].name, "cannonitemprobmult", 64);
  storedOptions[236].type = DOUBLETYPE;
  strlcpy(storedOptions[237].name, "randomitemprob", 64);
  storedOptions[237].type = DOUBLETYPE;
  strlcpy(storedOptions[238].name, "asteroidprob", 64);
  storedOptions[238].type = DOUBLETYPE;
  strlcpy(storedOptions[239].name, "asteroidconcentratorprob", 64);
  storedOptions[239].type = DOUBLETYPE;
  strlcpy(storedOptions[240].name, "asteroiditemprob", 64);
  storedOptions[240].type = DOUBLETYPE;
  strlcpy(storedOptions[241].name, "shotkillscoremult", 64);
  storedOptions[241].type = DOUBLETYPE;
  strlcpy(storedOptions[242].name, "torpedokillscoremult", 64);
  storedOptions[242].type = DOUBLETYPE;
  strlcpy(storedOptions[243].name, "smartkillscoremult", 64);
  storedOptions[243].type = DOUBLETYPE;
  strlcpy(storedOptions[244].name, "heatkillscoremult", 64);
  storedOptions[244].type = DOUBLETYPE;
  strlcpy(storedOptions[245].name, "clusterkillscoremult", 64);
  storedOptions[245].type = DOUBLETYPE;
  strlcpy(storedOptions[246].name, "laserkillscoremult", 64);
  storedOptions[246].type = DOUBLETYPE;
  strlcpy(storedOptions[247].name, "tankkillscoremult", 64);
  storedOptions[247].type = DOUBLETYPE;
  strlcpy(storedOptions[248].name, "runoverkillscoremult", 64);
  storedOptions[248].type = DOUBLETYPE;
  strlcpy(storedOptions[249].name, "ballkillscoremult", 64);
  storedOptions[249].type = DOUBLETYPE;
  strlcpy(storedOptions[250].name, "explosionkillscoremult", 64);
  storedOptions[250].type = DOUBLETYPE;
  strlcpy(storedOptions[251].name, "shovekillscoremult", 64);
  storedOptions[251].type = DOUBLETYPE;
  strlcpy(storedOptions[252].name, "crashscoremult", 64);
  storedOptions[252].type = DOUBLETYPE;
  strlcpy(storedOptions[253].name, "minescoremult", 64);
  storedOptions[253].type = DOUBLETYPE;
  strlcpy(storedOptions[254].name, "tankscoredecrement", 64);
  storedOptions[254].type = INTTYPE;
  strlcpy(storedOptions[255].name, "asteroidpoints", 64);
  storedOptions[255].type = DOUBLETYPE;
  strlcpy(storedOptions[256].name, "asteroidmaxscore", 64);
  storedOptions[256].type = DOUBLETYPE;
  strlcpy(storedOptions[257].name, "cannonpoints", 64);
  storedOptions[257].type = DOUBLETYPE;
  strlcpy(storedOptions[258].name, "cannonmaxscore", 64);
  storedOptions[258].type = DOUBLETYPE;
  strlcpy(storedOptions[259].name, "teamsharescore", 64);
  storedOptions[259].type = BOOLTYPE;
  strlcpy(storedOptions[260].name, "minitemmass", 64);
  storedOptions[260].type = DOUBLETYPE;
  return;
}
bool isOption(char name[64]) {
  int i;
  for (i=0; i<storedOptionCount; i++) {
    if (strcmp(name, storedOptions[i].name) == 0) {
      return True;
    }
  }
  return False;
}
int queueOption(options_struct option) {
  int i;
  for (i=0; i<queuedOptionCount; i++) {
    if (strcmp(queuedOptions[i].name, option.name) == 0) {
      return -1; //option have already been queued
    }
  }
  queuedOptions[i] = option;
  queuedOptionCount++;
  return 0; //option queued
}
int getOptionIndex(char name[64]) {
  int i;
  for (i=0; i<storedOptionCount; i++) {
    if (strcmp(storedOptions[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

void sendOptions() {
  int i;
  char message[64];
  for (i=0; i < MAX_MSGS && i < queuedOptionCount; i++) {
    strlcpy(message, "/get ", 64);
    strcat(message, queuedOptions[i].name);
    Net_talk(message);
    Send_talk();
  }
}
void recieveOptions() {
  int m,o,index, o2;
  int intResult;
  double doubleResult;
  char stringResult[64];
  char option[64];
  for (m=0; m<MAX_MSGS; m++) {
    if (strstr(TalkMsg[m]->txt, "The value of ") == NULL) {
      continue;
    }
    sscanf(TalkMsg[m]->txt, "The value of %s %*s", option);
    for (o=0; o<queuedOptionCount; o++) {
      if (strcmp(option, queuedOptions[o].name) == 0) {
        index=getOptionIndex(option);
        storedOptions[index].status = SET;
        switch (queuedOptions[o].type) {
          case INTTYPE:
            sscanf(TalkMsg[m]->txt, "The value of %*s is %d", &intResult);
            storedOptions[index].intValue = intResult;
            break;
          case DOUBLETYPE:
            sscanf(TalkMsg[m]->txt, "The value of %*s is %lf", &doubleResult);
            storedOptions[index].doubleValue = doubleResult;
            break;
          case STRINGTYPE:
            sscanf(TalkMsg[m]->txt, "The value of %*s is %s", stringResult);
            stringResult[strlen(stringResult)-1] = '\0';
            strlcpy(storedOptions[index].stringValue, stringResult, 64);
            break;
          case BOOLTYPE:
            sscanf(TalkMsg[m]->txt, "The value of %*s is %s", stringResult);
            if (strcmp(stringResult, "true.") == 0) {
              storedOptions[index].intValue = 1;
            }
            else
            {
              storedOptions[index].intValue = 0;
            }
            break;
          default:
            printf("ERROR: unknown type");
            break;
        }
        removeTalkMsg(m);
        queuedOptionCount--;
        for (o2=o; o2 < queuedOptionCount; o2++) {
          queuedOptions[o2] = queuedOptions[o2+1];
        }
      }
    }
  }
}

int getOption(char name[64]) {
  int i;
  if (!isOption(name)) {
    return -2;
  }
  for (i=0; i<storedOptionCount; i++) {
    if (strcmp(storedOptions[i].name, name) == 0) {
      if (storedOptions[i].status == SET) {
        return i;
      }
      else if (storedOptions[i].status == UNAVAILABLE) {
        return -4;
      }
      else if (storedOptions[i].status == UNSET) {
        queueOption(storedOptions[i]);
        return -5;
      }
      else
      {
        return -3;
      }
    }
    /*else
      /{
      queueOption(name);
      strlcpy(storedOptions[storedOptionCount].name, name, 64);
      return 0;
      }*/
  }
  return -1;
}


static PyObject* py_getOption(PyObject* pySelf, PyObject* args) {
  char *name[64];
  int result, i;
  if (!PyArg_ParseTuple(args, "s", &name)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  //convert to lowercase
  //I'm very bad at c-strings and have no idea
  //why i need double index on name, but it works. -hatten
  for (i=0; name[0][i]; i++) {
    name[0][i] = tolower(name[0][i]);
  }
  result = getOption(*name);
  switch(result) {
    case -5:
      return Py_BuildValue("s", "queued");
    case -4:
      PyErr_SetString(PyExc_TypeError, "Option unavailable");
      return NULL;
    case -3:
      PyErr_SetString(PyExc_TypeError, "unknown status");
      return NULL;
    case -2:
      PyErr_SetString(PyExc_TypeError, "option doesn't exist, or not supported");
      return NULL;
    case -1:
      PyErr_SetString(PyExc_TypeError, "generic error");
      return NULL;
    default:
      switch (storedOptions[result].type) {
        case INTTYPE:
        case BOOLTYPE:
          return Py_BuildValue("i", storedOptions[result].intValue);
        case DOUBLETYPE:
          return Py_BuildValue("d", storedOptions[result].doubleValue);
        case STRINGTYPE:
          return Py_BuildValue("s", storedOptions[result].stringValue);
        default:
          PyErr_SetString(PyExc_TypeError, "unknown type");
          return NULL;
      }
  }
}

//Utilizes Bresenham's line-drawing algorithm (no multiplication or division!) -EGG
//Adopted from http://www.brackeen.com/vga/source/djgpp20/lines.c.html (THANK YOU!) -EGG
//Parameters: x1, y1, x2, y2, flag to draw wall feelers, flag to draw wall detection. -EGG
//Returns distance between the first point and the wall if there is a wall between the two points or -1 if not. -EGG
//Removed detection drawing flags -CJG
//Uses doubles -hatten
double wallBetween(double x1, double y1, double x2, double y2) { //DO NOT CHANGE, NEEDED IN ORDER FOR WallFeeler & WallFeelerRad to work -JRA
  double i,sdx,sdy,dxabs,dyabs,x,y,px,py,ret;
  dxabs=abs(x2-x1);
  dyabs=abs(y2-y1);
  sdx=sgn(x2-x1);
  sdy=sgn(y2-y1);
  x=dyabs/2.0;
  y=dxabs/2.0;
  px=x1;
  py=y1;
  if (dxabs>=dyabs) { /* the line is more horizontal than vertical */
    for (i=0; i<dxabs; i++) {
      y+=dyabs;
      if (y >= dxabs) {
        y-=dxabs;
        py+=sdy;
      }
      px+=sdx;
      ret = wall_here(px, py);
      if (ret) return sqrt(pow((px-x1),2)+pow((py-y1),2));
    }
  }
  else { /* the line is more vertical than horizontal */
    for(i=0;i<dyabs;i++){
      x+=dxabs;
      if (x>=dyabs){
        x-=dyabs;
        px+=sdx;
      }
      py+=sdy;
      ret = wall_here(px, py);
      if (ret) return sqrt(pow((px-x1),2)+pow((py-y1),2));
    }
  }
  return -1;
}
static PyObject* py_wallBetween(PyObject* pySelf, PyObject* args) {
  double x1, y1, x2, y2;
  if (!PyArg_ParseTuple(args, "dddd", &x1, &y1, &x2, &y2)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  double result = wallBetween(x1, y1, x2, y2);
  if (result == -1)
    return Py_BuildValue("i",-1);
  else
    return Py_BuildValue("d", result);
}
//Shot functions
static PyObject* py_shotCountScreen(PyObject* pySelf, PyObject* args) {
  return Py_BuildValue("i",shotCount[0]);
}
static PyObject* py_shotX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < shotCount[0])
    return Py_BuildValue("i",AIshot[0][idx].x);
  else
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
  return NULL;
}
static PyObject* py_shotY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < shotCount[0])
    return Py_BuildValue("i",AIshot[0][idx].y);
  else
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
  return NULL;
}
static PyObject* py_shotDist(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < shotCount[0]) {
    int x = AI_wrap(pos.x, AIshot[0][idx].x, Setup->width);
    int y = AI_wrap(pos.y, AIshot[0][idx].y, Setup->height);
    return Py_BuildValue("d",AI_distance(pos.x, pos.y, x, y));
  }
  else
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
  return NULL;
}
static PyObject* py_shotVelX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < shotCount[0])
    return Py_BuildValue("d",AIshot[0][idx].ai.velX);
  else
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
  return NULL;
}
static PyObject* py_shotVelY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < shotCount[0])
    return Py_BuildValue("d",AIshot[0][idx].ai.velY);
  else
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
  return NULL;
}
static PyObject* py_shotAge(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < shotCount[0])
    return Py_BuildValue("i",AIshot[0][idx].ai.age);
  else
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
  return NULL;
}
static PyObject* py_shotSpeed(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIshot[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That shot is still fresh");
    return NULL;
  }
  else if (idx >= shotCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
    return NULL;
  }
  else
    return Py_BuildValue("d",AI_speed(AIshot[0][idx].ai.velX, AIshot[0][idx].ai.velY));
}
static PyObject* py_shotTrackingDeg(PyObject* pySelf, PyObject* args) {
  int idx, value;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIshot[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That shot is still fresh");
    return NULL;
  }
  else if (idx >= shotCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
    return NULL;
  }
  else {
    int velX = AIshot[0][idx].ai.velX;
    int velY = AIshot[0][idx].ai.velY;
    if (velX == 0 && velY == 0)
      return Py_BuildValue("i", 0);
    return Py_BuildValue("d", AI_radToDeg(atan2(velY, velX)));
  }
}
static PyObject* py_shotTrackingRad(PyObject* pySelf, PyObject* args) {
  int idx, value;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIshot[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That shot is still fresh");
    return NULL;
  }
  else if (idx >= shotCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
    return NULL;
  }
  else {
    int velX = AIshot[0][idx].ai.velX;
    int velY = AIshot[0][idx].ai.velY;
    if (velX == 0 && velY == 0)
      return Py_BuildValue("i", 0);
    return Py_BuildValue("d", atan2(velY, velX));
  }
}
static PyObject* py_shotAlert(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIshot[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That shot is still fresh");
    return NULL;
  }
  if (idx < shotCount[0])
    return Py_BuildValue("i",AIshot[0][idx].ai.alert);
  else {
    PyErr_SetString(PyExc_IndexError, "No shot with that id");
    return NULL;
  }
}
//asteroid functions -hatten
static PyObject* py_asteroidCountScreen(PyObject* pySelf, PyObject* args) {
  return Py_BuildValue("i",asteroidCount[0]);
}
static PyObject* py_asteroidX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].data.x);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].data.y);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidType(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].data.type);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidSize(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].data.size);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidRotation(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].data.rotation);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidVelX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("d",AIasteroid[0][idx].ai.velX);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidVelY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("d",AIasteroid[0][idx].ai.velY);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidAge(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].ai.age);
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidDist(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < asteroidCount[0])
  {
    int x = AI_wrap(pos.x, AIasteroid[0][idx].data.x, Setup->width);
    int y = AI_wrap(pos.y, AIasteroid[0][idx].data.y, Setup->height);
    return Py_BuildValue("d",AI_distance(pos.x, pos.y, x, y));
  }
  else
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
  return NULL;
}
static PyObject* py_asteroidSpeed(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIasteroid[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That asteroid is still fresh");
    return NULL;
  }
  else if (idx >= asteroidCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
    return NULL;
  }
  else
    return Py_BuildValue("d",AI_speed(AIasteroid[0][idx].ai.velX, AIasteroid[0][idx].ai.velY));
}
static PyObject* py_asteroidTrackingDeg(PyObject* pySelf, PyObject* args) {
  int idx;
  int value;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIasteroid[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That asteroid is still fresh");
    return NULL;
  }
  else if (idx >= asteroidCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
    return NULL;
  }
  else {
    int velX = AIasteroid[0][idx].ai.velX;
    int velY = AIasteroid[0][idx].ai.velY;
    if (velX == 0 && velY == 0)
      return Py_BuildValue("i", 0);
    return Py_BuildValue("d", AI_radToDeg(atan2(velY, velX)));
  }
}
static PyObject* py_asteroidTrackingRad(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIasteroid[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That asteroid is still fresh");
    return NULL;
  }
  if (idx >= asteroidCount[0]) {
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
    return NULL;
  }
  else {
    int velX = AIasteroid[0][idx].ai.velX;
    int velY = AIasteroid[0][idx].ai.velY;
    if (velX == 0 && velY == 0)
      return Py_BuildValue("i", 0);
    return Py_BuildValue("d", atan2(velY, velX));
  }
}
static PyObject* py_asteroidAlert(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)){
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (AIasteroid[0][idx].ai.age == 0) {
    PyErr_SetString(PyExc_ValueError, "That asteroid is still fresh");
    return NULL;
  }
  if (idx < asteroidCount[0])
    return Py_BuildValue("i",AIasteroid[0][idx].ai.alert);
  else {
    PyErr_SetString(PyExc_IndexError, "No asteroid with that id");
    return NULL;
  }
}
//moar functions
int phasingTime(void) {
  return phasingtimemax;
}
int getNextCheckPoint(void) {
  return nextCheckPoint;
}
static PyObject* py_checkPointX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < 26 && idx >= 0) {
    return Py_BuildValue("i",checks[idx].pos / Setup->y * BLOCK_SZ);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_checkPointY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < 26 && idx >= 0) {
    return Py_BuildValue("i",checks[idx].pos % Setup->y * BLOCK_SZ);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_checkPointBlockX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < 26 && idx >= 0) {
    return Py_BuildValue("i",checks[idx].pos / Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_checkPointBlockY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < 26 && idx >= 0) {
    return Py_BuildValue("i",checks[idx].pos % Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}


//connector functions -hatten
//a connector is whats between you and the ball
static PyObject* py_connectorCountScreen(PyObject* pySelf, PyObject* args) {
  return Py_BuildValue("i",num_connector);
}
static PyObject* py_connectorX0(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_connector && idx >= 0) {
    return Py_BuildValue("i",connector_ptr[idx].x0);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_connectorX1(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_connector && idx >= 0) {
    return Py_BuildValue("i",connector_ptr[idx].x1);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_connectorY0(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_connector && idx >= 0) {
    return Py_BuildValue("i",connector_ptr[idx].y0);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_connectorY1(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_connector && idx >= 0) {
    return Py_BuildValue("i",connector_ptr[idx].y1);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_connectorTractor(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_connector && idx >= 0) {
    return Py_BuildValue("i",connector_ptr[idx].tractor);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

//Missile functions -hatte
int missileCountScreen(void) {
  return num_missile;
}
static PyObject* py_missileX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_missile && idx >= 0) {
    return Py_BuildValue("i",missile_ptr[idx].x);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_missileY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_missile && idx >= 0) {
    return Py_BuildValue("i",missile_ptr[idx].y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_missileHeadingXdeg(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_missile && idx >= 0) {
    return Py_BuildValue("i",missile_ptr[idx].dir);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_missileHeadingRad(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_missile && idx >= 0) {
    return Py_BuildValue("d",AI_xdegToRad(missile_ptr[idx].dir));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_missileHeadingDeg(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_missile && idx >= 0) {
    return Py_BuildValue("d",AI_xdegToDeg(missile_ptr[idx].dir));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_missileLen(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_missile && idx >= 0) {
    return Py_BuildValue("i",missile_ptr[idx].len);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

//Laser functions -hatten
int laserCountScreen(void) {
  return num_laser;
}
static PyObject* py_laserX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_laser && idx >= 0) {
    return Py_BuildValue("i",laser_ptr[idx].x);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_laserY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_laser && idx >= 0) {
    return Py_BuildValue("i",laser_ptr[idx].y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_laserHeadingXdeg(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_laser && idx >= 0) {
    return Py_BuildValue("i",laser_ptr[idx].dir);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_laserHeadingRad(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_laser && idx >= 0) {
    return Py_BuildValue("d",AI_xdegToRad(laser_ptr[idx].dir));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_laserHeadingDeg(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_laser && idx >= 0) {
    return Py_BuildValue("d",AI_xdegToDeg(laser_ptr[idx].dir));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_laserLen(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_laser && idx >= 0) {
    return Py_BuildValue("i",laser_ptr[idx].len);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

//Ball Functions - hatten
int ballCountScreen(void) {
  return num_ball;
}
static PyObject* py_ballX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_ball && idx >= 0) {
    return Py_BuildValue("i",ball_ptr[idx].x);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

static PyObject* py_ballY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_ball && idx >= 0) {
    return Py_BuildValue("i",ball_ptr[idx].y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_ballId(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_ball && idx >= 0) {
    return Py_BuildValue("i",ball_ptr[idx].id);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

//shiphandling elsewhere

//Mine functions -hatten
int mineCountScreen(void) {
  return num_mine;
}
static PyObject* py_mineX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_mine && idx >= 0) {
    return Py_BuildValue("i",mine_ptr[idx].x);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_mineY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_mine && idx >= 0) {
    return Py_BuildValue("i",mine_ptr[idx].y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_mineFriendly(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_mine && idx >= 0) {
    return Py_BuildValue("i",mine_ptr[idx].teammine);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_mineId(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_mine && idx >= 0) {
    return Py_BuildValue("i",mine_ptr[idx].id);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

//itemhandling is elsewhere (should it be?) -hatten

//wreckage would add 6 functions and are only interesting
//if wreckage can kill, which is seldom set AFAIK -hatten




//wormhole functions -hatten
int wormholeCountScreen(void) {
  return num_wormholes;
}
static PyObject* py_wormholeX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_wormholes && idx >= 0) {
    return Py_BuildValue("i",wormhole_ptr[idx].x);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_wormholeY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_wormholes && idx >= 0) {
    return Py_BuildValue("i",wormhole_ptr[idx].y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

//ECM functions -hatten
int ecmCountScreen(void) {
  return num_ecm;
}
static PyObject* py_ecmX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_ecm && idx >= 0) {
    return Py_BuildValue("i",ecm_ptr[idx].x);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_ecmY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_ecm && idx >= 0) {
    return Py_BuildValue("i",ecm_ptr[idx].y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}


//paused are uninteresting, we alreade have pausedCountServer
//radar fixed elsewhere
int timeLeftSec(void) {
  return time_left;
}

//fuelstation functions
int fuelStationCount(void) {
  return num_fuels;
}
static PyObject* py_fuelStationX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_fuels && idx >= 0) {
    return Py_BuildValue("i",fuels[idx].pos / Setup->y*BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_fuelStationY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_fuels && idx >= 0) {
    return Py_BuildValue("i",fuels[idx].pos % Setup->y*BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_fuelStationBlockX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_fuels && idx >= 0) {
    return Py_BuildValue("i",fuels[idx].pos / Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_fuelStationBlockY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_fuels && idx >= 0) {
    return Py_BuildValue("i",fuels[idx].pos % Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_fuelStationFuel(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_fuels && idx >= 0) {
    return Py_BuildValue("i",(int)(fuels[idx].fuel/252));
  } //252 is a really strange value, from where does it come? -h
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
//There is no way of knowing if fuelstations are teambound
//aside from asking the server. Do it with getOption()
static PyObject* py_fuelStationTeam(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (!BIT(Setup->mode, TEAM_PLAY)) {
    PyErr_SetString(PyExc_LookupError, "Team play not enabled");
    return NULL;
  }
  if (idx < num_fuels && idx >= 0) {
    return Py_BuildValue("i",Find_closest_team(fuels[idx].pos / Setup->y, fuels[idx].pos % Setup->y));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}

int cannonCountServer(void) {
  return num_cannons;
}
static PyObject* py_cannonX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_cannons && idx >= 0) {
    return Py_BuildValue("i",cannons[idx].pos / Setup->y * BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_cannonY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_cannons && idx >= 0) {
    return Py_BuildValue("i",cannons[idx].pos % Setup->y * BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_cannonBlockX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_cannons && idx >= 0) {
    return Py_BuildValue("i",cannons[idx].pos / Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_cannonBlockY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_cannons && idx >= 0) {
    return Py_BuildValue("i",cannons[idx].pos % Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_cannonAlive(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_cannons && idx >= 0) {
    return Py_BuildValue("i",cannons[idx].dead_time ? 1 : 0);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
//cannons[idx].dot only tells if a dot should be painted
//inside it, afaik, so not interesting -hatten

//There is no way to know if cannons are teambound aside from
//asking the server. The player can check it with getOption.
static PyObject* py_cannonTeam(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (!BIT(Setup->mode, TEAM_PLAY)) {
    PyErr_SetString(PyExc_LookupError, "Team play not enabled");
    return NULL;
  }
  if (idx < num_cannons  && idx >= 0) {
    return Py_BuildValue("i",Find_closest_team(cannons[idx].pos / Setup->y, cannons[idx].pos % Setup->y));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
int targetCountServer(void) {
  return num_targets;
}
static PyObject* py_targetX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("i",(targets[idx].pos / Setup->y)*BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_targetY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("i",(targets[idx].pos % Setup->y)*BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_targetBlockX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("i",(targets[idx].pos / Setup->y));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_targetBlockY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("i",(targets[idx].pos % Setup->y));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_targetAlive(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("i",targets[idx].dead_time == 0 ? 1 : 0);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
//damage only updates if you see it or if it dies
static PyObject* py_targetDamage(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("d",(double)targets[idx].damage/6400.0);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
//if teamplay is on, targets will be teambound. -hatten
static PyObject* py_targetTeam(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (!BIT(Setup->mode, TEAM_PLAY)) {
    PyErr_SetString(PyExc_LookupError, "Team play not enabled");
    return NULL;
  }
  if (idx < num_targets && idx >= 0) {
    return Py_BuildValue("i",Find_closest_team(targets[idx].pos / Setup->y, targets[idx].pos % Setup->y));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
int baseCountServer(void) {
  return num_bases;
}
static PyObject* py_baseX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_bases && idx >= 0) {
    return Py_BuildValue("i",bases[idx].pos / Setup->y * BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_baseY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_bases && idx >= 0) {
    return Py_BuildValue("i",bases[idx].pos % Setup->y * BLOCK_SZ + (int)(BLOCK_SZ*0.5));
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_baseBlockX(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_bases && idx >= 0) {
    return Py_BuildValue("i",bases[idx].pos / Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_baseBlockY(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_bases && idx >= 0) {
    return Py_BuildValue("i",bases[idx].pos % Setup->y);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_baseId(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_bases && idx >= 0) {
    return Py_BuildValue("i",bases[idx].id);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
static PyObject* py_baseTeam(PyObject* pySelf, PyObject* args) {
  int idx;
  if (!PyArg_ParseTuple(args, "i", &idx)) {
    PyErr_SetString(PyExc_TypeError, "invalid parameter");
    return NULL;
  }
  if (idx < num_bases && idx >= 0) {
    return Py_BuildValue("i",bases[idx].team);
  }
  PyErr_SetString(PyExc_IndexError, "invalid value for index");
  return NULL;
}
//vcannon, vfuel, vbase, vdecor removed. Don't see what they do

//Methods to help AI loop -JNE
void calcStuff(int j) {     //updates data in allShips for velocity and tracking in degrees and radians -JNE
  allShips[j][0].d = sqrt(pow(wrapX(allShips[j][0].ship.x,pos.x)-pos.x,2)+pow(wrapX(allShips[j][0].ship.y,pos.y)-pos.y,2));
  allShips[j][0].vel = sqrt(pow(wrapX(allShips[j][0].ship.x,allShips[j][2].ship.x)-allShips[j][2].ship.x,2)+pow(wrapY(allShips[j][0].ship.y,allShips[j][2].ship.y)-allShips[j][2].ship.y,2))/2;   //calculate velocity
  allShips[j][0].velX = wrapX(allShips[j][0].ship.x,allShips[j][2].ship.x)-allShips[j][2].ship.x; //calculate x velocity
  allShips[j][0].velY = wrapY(allShips[j][0].ship.y,allShips[j][2].ship.y)-allShips[j][2].ship.y; //calculate y velocity

  if (allShips[j][0].velX == 0 && allShips[j][0].velY == 0) {
    allShips[j][0].trackingRad = AI_xdegToRad(allShips[j][0].ship.dir);
    allShips[j][0].trackingDeg = AI_xdegToDeg(allShips[j][0].ship.dir);
  }
  else {
    allShips[j][0].trackingRad = atan2(allShips[j][0].velY,allShips[j][0].velX); //calculate tracking
    allShips[j][0].trackingDeg = AI_radToDeg(allShips[j][0].trackingRad);
  }
}
void updateSlots() {  //moves everything in allShips over by a frame -JNE
  int i;
  ship_t theShip;
  theShip.x=-1;
  theShip.y=-1;
  theShip.dir=-1;
  theShip.shield=-1;
  theShip.id=-1;
  for (i=0;i<128;i++) {     //check every slot in allShips
    if (allShips[i][0].vel!=-1 || allShips[i][1].vel!=-1 || allShips[i][2].vel!=-1) { //only update slots that were updated in the last three frames
      allShips[i][2] = allShips[i][1];  //bump the last two down one
      allShips[i][1] = allShips[i][0];
      allShips[i][0].vel = -1;    //this is updated later if the ship is still on screen
      allShips[i][0].d = 9999;
      allShips[i][0].velX=-1;
      allShips[i][0].velY=-1;
      allShips[i][0].trackingDeg=-1;
      allShips[i][0].trackingRad=-1;
      /*if (allShips[i][1].reload > 0) allShips[i][0].reload=allShips[i][1].reload-1; //reload tracking -EGG
        else if (allShips[i][1].vel!=-1) allShips[i][0].reload=0;
        else allShips[i][0].reload=-1;*/
      allShips[i][0].ship=theShip;
    }
  }
}
int updateFirstOpen() { //goes through allShips, returning the index of the first open spot -JNE
  int i;
  for (i=0;i<128;i++) {
    if (allShips[i][0].vel==-1 && allShips[i][1].vel==-1 && allShips[i][2].vel==-1) {
      return i;
    }
  }
  return -1;
}
bool updateShip(int i) { //goes through allShips and checks if a particular ship is there, returning true if it is and false if it isn't -JNE
  int j;
  for (j=0;j<128;j++) {
    if (ship_ptr[i].id==allShips[j][1].ship.id) {   //find the spot where the ship's ID is located
      allShips[j][0].ship = ship_ptr[i];
      if (allShips[j][2].vel >= 0) {
        calcStuff(j);
      }
      else {
        allShips[j][0].vel = 0;
      }
      return true;            //ship was found, so don't add it as a new ship
    }
  }
  return false;
}
void addNewShip(int firstOpen, int i) { //add a ship that has never been on screen before -JNE
  if (selfID() != ship_ptr[i].id) {
    if (updateShip(i)==false) {
      allShips[firstOpen][0].ship = ship_ptr[i];
      allShips[firstOpen][0].vel = 0;
    }
  }
}
int sortShips() { //sorts the ships in the allShips buffer by how far away they are from the player -JNE
  //See our previous quicksort thanks ;)
#define  MAX_LEVELS  1000
  shipData_t piv;
  int  beg[MAX_LEVELS], end[MAX_LEVELS], i=0, L, R ;
  beg[0]=0; end[0]=128;
  while (i>=0) {
    L=beg[i]; R=end[i]-1;
    if (L<R) {
      piv=allShips[L][0]; if (i==MAX_LEVELS-1) return -1;
      while (L<R) {
        while (allShips[R][0].d>=piv.d && L<R) R--;
        if (L<R) {
          allShips[L++][0]=allShips[R][0];
          allShips[L][1]=allShips[R][1];
          allShips[L][2]=allShips[R][2];
        }
        while (allShips[L][0].d<=piv.d && L<R) L++;
        if (L<R) {
          allShips[R--][0]=allShips[L][0];
          allShips[R][1]=allShips[L][1];
          allShips[R][2]=allShips[L][2];
        }
      }
      allShips[L][0]=piv; beg[i+1]=L+1; end[i+1]=end[i]; end[i++]=L; }
    else {
      i--;
    }
  }
  return 1;
}
//update ships' velocity and tracking
void prepareShips() {
  updateSlots();          //move all the ship data one slot (or frame) to the right -JNE
  int firstOpen;
  firstOpen = 0;
  int i;
  for (i=0;i<num_ship;i++) {      //go through each ship on screen, updating their position and adding them if they are not there -JNE
    firstOpen = updateFirstOpen();
    addNewShip(firstOpen, i);
  }
  sortShips();
  if (reload > 0) reload--;
}
//End of methods to help AI_loop -JNE
//THE L00PZ -EGG
static PyObject* py_switchLoop(PyObject* pySelf, PyObject* args) {
  PyObject *temp;
  if (PyArg_ParseTuple(args, "O:set_callback", &temp)) {
    if (!PyCallable_Check(temp)) {
      PyErr_SetString(PyExc_TypeError, "parameter must be callable");
      return NULL;
    }
    Py_XINCREF(temp);         /* Add a reference to new callback */
    Py_XDECREF(py_loop);  /* Dispose of previous callback */
    py_loop = temp;       /* Remember new callback */
  }
  Py_RETURN_NONE;
}
void AI_loop() {
  if (PyCallable_Check(py_loop)) PyObject_CallObject(py_loop,NULL);
}
void release_keys() {
  int i;
  for (i=0; i < pressedKeyCount; i++) {
    Keyboard_button_released(pressedKeys[i]);
  }
  pressedKeyCount = 0;
}

//END L00PZ -EGG
//Inject our loop -EGG
void injectAI() { 
  prepareShips();
  AIasteroid_refresh(); //hatten
  AIshot_refresh(); //hatten
  AIitem_refresh(); //hatten
  release_keys(); //added to make thrust etc toggles -hatten
  recieveOptions();
  if (AI_delaystart % 5 == 0)
    sendOptions();
  if (AI_delaystart == 0) {
    fillOptions();
    getOption("firerepeatrate");
    getOption("randomitemprob");
  }
  AI_loop();
  AI_delaystart++;
}
//END inject -EGG
//Run xpilot without a window -EGG translate -CJG
void headlessMode() {
  headless=1;
}
//Oh glorious py_main(), with the regular main(), you just start the Python shell. -.-; -EGG
static PyObject* py_start(PyObject* pySelf, PyObject* args) {
  int j,k;
  ship_t theShip;
  theShip.x=-1;
  theShip.y=-1;
  theShip.dir=-1;
  theShip.shield=-1;
  theShip.id=-1;
  for (j=0;j<128;j++) { //Initialize allShips for enemy velocity
    for (k=0;k<3;k++) {
      allShips[j][k].vel=-1;
      allShips[j][k].d=9999;    //needs to be arbitrarily high so that it is sorted correctly in allShips
      allShips[j][k].ship = theShip;
      allShips[j][k].velX=-1;
      allShips[j][k].velY=-1;
      allShips[j][k].trackingDeg=-1;
      allShips[j][k].trackingRad=-1;
      //allShips[j][k].reload=-1;
    }
  }
  AI_delaystart = 0;
  //AIshot_reset();
  //AIshot_toggle = 1;
  AI_alerttimemult = 5;
  printf("\n~~~~~~~~~~~~~~~~~~~~~~~~\nAI INTERFACE INITIALIZED\n~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
  //Parse python arguments
  int i;
  PyObject* listObj = PyList_New(0);
  PyObject* strObj;
  PyObject* temp;
  if (!PyArg_ParseTuple(args, "O|O", &temp, &listObj))
    return Py_BuildValue("s","INVALID ARGUMENTS!\n");
  int argc = PyList_Size(listObj)+1;
  char* argv[argc];
  argv[0] = "xpilot-ng-x11";
  for (i=0; i<argc-1; i++) {
    strObj = PyList_GetItem(listObj, i);
    strObj = PyUnicode_AsEncodedString(strObj,"utf-8","strict");
    argv[i+1] = PyBytes_AS_STRING(strObj);
  }
  //Set AI loop
  args = Py_BuildValue("(O)",temp);
  py_switchLoop(pySelf, args);
  return Py_BuildValue("i",main(argc,argv));
}
char* getAiVersion(void) {
  return "Xpilot-AI-fork 1.0 20140702";
}