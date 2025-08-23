// XJTFANIMATE ; ; ;2025-08-08 12:20:07;11.6
// 	GOTO MAIN
// 	;*********************************************************************
// 	; PURPOSE:  ANIMATION EDITOR
// 	; AUTHOR:   JACK FOX
// 	; REVISIONS:
// 	;  *JTF 06/25 Created
// 	;*********************************************************************
// 	;

#include "vec3.hh"
#include "vec2.hh"
#include "quat.hh"
#include "util.hh"
#include "render.hh"
#include <vector>

typedef vec3<uint8_t> color;

struct vertex {
	vec3f pos;
	color col;
};

vertex lerp(const vertex& v1, const vertex& v2, float t) {
	return { lerp(v1.pos, v2.pos, t), lerp(v1.col, v2.col, t) };
}

struct keyframe {
	vec3f pos;
	quaternion transform;
	unsigned frame_time;
};

struct object {

	std::vector<vertex> vertices;
	std::vector<std::vector<size_t>> lines;

	std::vector<keyframe> keyframes;

	vec3f position = { 0, 0, 0 };
	quaternion transform = { 0, 0, 1, 0 };

	unsigned keyframe = 0;
	unsigned frame_timer = 0;

};

std::vector<object> objects{
	{
		{
			{ { -1,-1,-1 }, {   0,   0,   0 } },
			{ {  1,-1,-1 }, { 255,   0,   0 } },
			{ { -1, 1,-1 }, {   0, 255,   0 } },
			{ {  1, 1,-1 }, { 255, 255,   0 } },
			{ { -1,-1, 1 }, {   0,   0, 255 } },
			{ {  1,-1, 1 }, { 255,   0, 255 } },
			{ { -1, 1, 1 }, {   0, 255, 255 } },
			{ {  1, 1, 1 }, { 255, 255, 255 } },
		},
		{
			{ 0, 1, 3, 2, 0 },
			{ 4, 5, 7, 6, 4 },
			{ 0, 4 }, { 1, 5 },
			{ 3, 7 }, { 2, 6 },
		},
		{
			{
				{ 0, 0, 5 },
				quaternion::axis_angle({ 0, 1, 0 }, 0),
				10,
			},
			{
				{ 1, 1, 5 },
				quaternion::axis_angle({ 0, 1, 0 }, M_PI),
				10,
			}
		},
	}
};

// 	; DESCRIPTION: PARSE A PARAMETER PROPERTY STRING OUT INTO ITS PARTS
// 	; PARAMETERS:
// 	;   PARAMS    (I,REQ) - A ; DELIMITED LIST OF ^ DELIMITED PARAMETER PROPERTY STRINGS
// 	;   INDEX     (I,REQ) - THE INDEX OF THE PARAMETER TO PARSE
// 	;   PARAM     (O,REQ) - THE NAME OF THE VARIABLE THAT THE PARAMETER IS BOUND TO, $N FOR A SPECIFIC PIECE
// 	;   NAME      (O,OPT) - THE NAME THAT SHOULD BE SHOWN FOR THE PARAMETER, IF DIFFERENT THAN THE BOUND VARIABLE NAME
// 	;   COLUMN    (O,REQ) - THE COLUMN ON THE PARAMETER BAR THE PARAMETER SHOULD BE DISPLAYED AT
// 	;   INCREMENT (O,OPT) - THE AMOUNT THAT THE BOUND VARIABLE SHOULD BE INCREMENTED OR DECREMENTED BY USING THE UP AND DOWN ARROWS
// 	;   MIN       (O,OPT) - THE MINIMUM VALUE THE BOUND VARIABLE SHOULD BE ALLOWED TO HAVE
// 	;   MAX       (O,OPT) - THE MAXIMUM VALUE THE BOUND VARIABLE SHOULD BE ALLOWED TO HAVE
// 	;   LINE      (O,OPT) - THE LINE THAT THE PARAMETER SHOULD BE DISPLAYED ON, RELATIVE TO THE FIRST LINE OF THE PARAMETER BAR
// PARSEPARAM(PARAMS,INDEX,PARAM,NAME,COLUMN,INCREMENT,MIN,MAX,LINE) ;
// 	NEW PIECE
// 	SET PIECE=$PIECE(PARAMS,";",INDEX)
// 	SET PARAM=$PIECE(PIECE,"^",1)
// 	SET NAME=$PIECE(PIECE,"^",2)
// 	SET COLUMN=$PIECE(PIECE,"^",3)
// 	SET INCREMENT=$PIECE(PIECE,"^",4)
// 	SET MIN=$PIECE(PIECE,"^",5)
// 	SET MAX=$PIECE(PIECE,"^",6)
// 	SET LINE=$PIECE(PIECE,"^",7)
// 	QUIT


// LOADSCENE(NAME,OBJECTS) ;
// 	NEW LIST ;
// 	WRITE *27,"[3H",*27,"[2K"
// LNA WRITE *27,"[2H",*27,"[2K"
// 	WRITE "SCENE NAME: "
// 	SET NAME=$$zScrReadLine(.CTL,20,"","",NAME,1)
// 	QUIT:CTL="UAR"
// 	IF NAME="?" DO  GOTO LNA
// 	. IF 'LIST WRITE !,*27,"[2K",?2,"ENTER THE NAME OF A SCENE TO LOAD.",!?2,"ENTER ? AGAIN TO SEE A LIST OF LOADED SCENES." SET LIST=1
// 	. ELSE  WRITE !,*27,"[2K",?2,"SCENES:" KILL NAME FOR  SET NAME=$ORDER(^XJTFANIMATE(NAME)) QUIT:NAME=""  WRITE !,*27,"[2K",?4,NAME
// 	QUIT:NAME=""
// 	IF '$DATA(^XJTFANIMATE(NAME)) WRITE !,"PROGRAM DOES NOT EXIST." GOTO LNA
// 	MERGE OBJECTS=^XJTFANIMATE(NAME)
// 	QUIT
// 	;
// SAVEDATA(NAME,OBJECTS) ;
// 	NEW GLO,OBJ,SUB
// 	SET GLO=$NAME(^XJTFANIMATE(NAME))
// 	;
// 	SET @GLO=OBJECTS
// 	FOR OBJ=1:1:OBJECTS DO
// 	. FOR SUB="V","L","A" DO  ; ONLY SAVE STATIC DATA
// 	. . MERGE @GLO@(OBJ,SUB)=OBJECTS(OBJ,SUB)
// 	;
// 	QUIT
// 	;


const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

void init();
void mainLoop();
void tick();
void Render(SDL_Renderer* sdlRenderer);
void hud(SDL_Renderer* sdlRenderer);

SDL_Renderer* sdlRenderer;
SDL_Texture* targetTexture;

// const int param_rows = 4;
// const int param_cols = 5;
// int param_index = 0;
// float *params[param_rows * param_cols] = { &alignmentRadius, &cohesionRadius, &avoidanceRadius, &flockRadius, &obstacleRadius,
//                                            &alignmentWeight, &cohesionWeight, &avoidanceWeight, &flockWeight, &obstacleWeight,
//                                            &maxSpeed,        &maxAccel,       nullptr,          &dragCoeff,      &edgeObstacle, 
//                                            &minSpeed,        &baseAccel,      nullptr,          nullptr,         nullptr };
// float delta[param_rows * param_cols];

bool running = true;
bool advance = false;
bool step = true;

int zoom = 0;
vec2f zoom_pos = { 0, 0 };

// int height = 24, width = 80; // default terminal size
// 	new vertex,v1,v2
// 	new vertices,line
// 	new x,y,z,x1,y1,z1,x2,y2,z2,c1,c2
// 	new px,py,pz,scene
// 	new depthbuffer,zclip
// 	new frame,nextframe,t
// 	;
// 	; editor state data
bool pause = false, prev = false, next = false;
// 	new input,char,pause,next,prev,add,save,load
bool quit, skip;
unsigned sel, obj;
std::vector<unsigned> updated;
// 	new params,i
// 	set obj=1,sel=1
// 	;

// 	
float vscale = WINDOW_HEIGHT; // ; window scale
float hscale = WINDOW_WIDTH;
float zclip = 0.01; // distance from camera to near clip plane


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int main(int argc, char* argv[]) { // args are required for SDL_main
#pragma GCC diagnostic pop



	SDL_version compiled;
	SDL_version linked;
	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);
	printf("Compiled against SDL version %d.%d.%d\n",
		compiled.major, compiled.minor, compiled.patch);
	printf("Linked against SDL version %d.%d.%d.\n",
		linked.major, linked.minor, linked.patch);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		logSDLError("Init");
		return EXIT_FAILURE;
	}

	SDL_Window* window = SDL_CreateWindow("Hello, Boids!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
	if (window == nullptr) {
		logSDLError("CreateWindow");
		SDL_Quit();
		return EXIT_FAILURE;
	}

	sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	if (sdlRenderer == nullptr) {
		cleanup(window);
		logSDLError("CreateRenderer");
		SDL_Quit();
		return EXIT_FAILURE;
	}

	targetTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);
	if (targetTexture == nullptr) {
		cleanup(window, sdlRenderer);
		logSDLError("CreateTexture");
		SDL_Quit();
		return EXIT_FAILURE;
	}

	init();

	

#if __EMSCRIPTEN__
	emscripten_set_main_loop(mainLoop, 0, true);
#else
	while (running) mainLoop();
#endif

	cleanup(sdlRenderer, window, targetTexture);
	SDL_Quit();
	return 0;
}

void init()
{
	objects[0].position = { 0, 0, 5 };
}

void mainLoop()
{
	if (advance) {
		tick();
		if (step) { advance = false; }
	}

	// Draw the screen
	SDL_SetRenderTarget(sdlRenderer, targetTexture);

	SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(sdlRenderer);

	Render(sdlRenderer);

	SDL_SetRenderTarget(sdlRenderer, nullptr);

	const float zoom_sens = 0.25f;
	float zoom_mul = std::pow(2.f, -zoom * zoom_sens);
	SDL_Rect zoom_window = { static_cast<int>(zoom_pos.x),
							 static_cast<int>(zoom_pos.y),
							 static_cast<int>(WINDOW_WIDTH * zoom_mul),
							 static_cast<int>(WINDOW_HEIGHT * zoom_mul) };

	SDL_RenderCopy(sdlRenderer, targetTexture, &zoom_window, nullptr);

	hud(sdlRenderer);

	SDL_RenderPresent(sdlRenderer);

}

// 	;
// 	;
// 	set scene="default"
// 	do initdefault(.objects)
// 	;
// 	write *27,"%@",*27,"[?25l",#  ; set codepage default, disable cursor, clear screen
// 	do %zecho(0)


	// 	. do getscreensize^xjtfterm(.height,.width) ; check for resize
	// 	. write # // clear screen
	// 	. kill depthbuffer
	// 	. ;

void tick() {
	for (object& object : objects) {

		// advance the animation
		if (!pause || next || prev) {

			if (!pause) --object.frame_timer; // count down the frame timer
			if (pause && next) {
				next = false;
				object.frame_timer = 0; // advance to next keyframe
			}
			if (pause && prev) {
				prev = false;
				// if mid-frame and prev pressed, jump to start of keyframe
				if (object.frame_timer < object.keyframes[object.keyframe].frame_time) {
					object.frame_timer = object.keyframes[object.keyframe].frame_time;
				}
				else { // otherwise go back 1 keyframe
					if (object.keyframe > 0) object.keyframe--;
					else object.keyframe = object.keyframes.size() - 1; // loop back to the end if needed
					object.frame_timer = object.keyframes[object.keyframe].frame_time; // load the frame timer from the keyframe time
				}

			}
			if (object.frame_timer <= 0) {  // if it hit zero
				object.keyframe++; // advance to the next keyframe
				if (object.keyframe >= object.keyframes.size()) object.keyframe = 0; // loop animation
				object.frame_timer = object.keyframes[object.keyframe].frame_time; // load the frame timer from the keyframe time
			}

			// update position and rotation
			unsigned frame = object.keyframe;
			unsigned nextframe = frame + 1;
			if (nextframe > object.keyframes.size()) nextframe = 0;
			float t = static_cast<float>(object.frame_timer) / object.keyframes[frame].frame_time;
			object.position = lerp(object.keyframes[frame].pos, object.keyframes[nextframe].pos, 1 - t);
			object.transform = onlerp(object.keyframes[frame].transform, object.keyframes[nextframe].transform, 1 - t);
		}
	}
}


void drawline(SDL_Renderer* sdlRenderer, vertex v1, vertex v2) {

	// std::cout << "drawing line " << v1.pos << " to " << v2.pos << std::endl;
	// TODO: update SDL so this can use RenderGeometry
	float dt = 1 / std::max(std::abs(v2.pos.x - v1.pos.x), std::abs(v2.pos.y - v1.pos.y));
	vertex v = v1;
	for (float t = 0; t < 1; t += dt) {
		v = lerp(v1, v2, t);
		SDL_SetRenderDrawColor(sdlRenderer, v.col.x, v.col.y, v.col.z, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawPointF(sdlRenderer, v.pos.x, v.pos.y);
	}


}


void Render(SDL_Renderer* sdlRenderer) {
	for (const object& object : objects) {

		// render the object
		std::vector<vertex> vertices(object.vertices);
		for (vertex& vertex : vertices) { // transform vertices
			vertex.pos = apply(vertex.pos, object.transform) + object.position;
		}
		for (const std::vector<size_t>& line : object.lines) {
			for (size_t l = 0; l < line.size() - 1; l++) {
				vertex v1 = vertices[line[l]];
				vertex v2 = vertices[line[l + 1]];

				// std::cout << "clipping line " << v1.pos << " to " << v2.pos << std::endl;

				if (v1.pos.z <= zclip && v2.pos.z <= zclip) continue;
				if (v2.pos.z <= zclip) {
					float x = (v2.pos.x - v1.pos.x);
					float y = (v2.pos.y - v1.pos.y);
					float z = (v1.pos.z - zclip) / (v1.pos.z - v2.pos.z);
					v2.pos.z = zclip;
					v2.pos.x = v1.pos.x + (z * x);
					v2.pos.y = v1.pos.y + (z * y);
				}
				if (v1.pos.z <= zclip) {
					float x = (v1.pos.x - v2.pos.x);
					float y = (v1.pos.y - v2.pos.y);
					float z = (v2.pos.z - zclip) / (v2.pos.z - v1.pos.z);
					v1.pos.z = zclip;
					v1.pos.x = v2.pos.x + (z * x);
					v1.pos.y = v2.pos.y + (z * y);
				}
				v1.pos.x = v1.pos.x * hscale / v1.pos.z + (WINDOW_WIDTH / 2), v1.pos.y = v1.pos.y * vscale / v1.pos.z + (WINDOW_HEIGHT / 2);
				v2.pos.x = v2.pos.x * hscale / v2.pos.z + (WINDOW_WIDTH / 2), v2.pos.y = v2.pos.y * vscale / v2.pos.z + (WINDOW_HEIGHT / 2);
				drawline(sdlRenderer, v1, v2);
			}
		}

	}

}


void hud(SDL_Renderer* sdlRenderer) {


	// 	. if pause do
	// 	. . write *27,"[",3,"h","rotate current keyframe: h/k - yaw  u/j - pitch  y/i - roll"
	// 	. ; handle input and hud elements

	// 	. kill input,updated

	// 	. for  read *char:0 quit:char=-1  set input=input+1,input(input)=char,input(input,"key")=$key ; read input buffer 1 char at a time
	// 	. ;
	// 	. do hotkeybar(1,$name(quit)_";"_$name(save)_";"_$name(load)_";"_$name(pause)_$s(pause:";"_$name(prev)_"^[prev kf;"_$name(next)_"^]next kf;"_$name(add)_"^add kf",1:""),.input)
	// 	. ;
	// 	. do statusbar(height-2,$name(objects(obj,"f"))_"^keyframe^1;"_$name(objects(obj,"t"))_"^time^15;")
	// 	. ;
	// 	. set objp=$name(objects(obj,"a",objects(obj,"f"),"p"))
	// 	. set params=$name(obj)_"^^1^1^1^"_objects_$s(pause:";"_$name(objects(obj,"a",objects(obj,"f"),"t"))_"^frametime^8^1^1;"_objp_"$1^x^25^1;"_objp_"$2^y^30^1;"_objp_"$3^z^35^1",1:"")
	// 	. do paramsbar(2,params,.sel,.input,.updated)

	quaternion& objq = objects[obj].keyframes[objects[obj].keyframe].transform;

	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type) {
		case SDL_QUIT:
			running = false;
			break;
		case SDL_KEYDOWN:
			switch (ev.key.keysym.sym) {

			case SDLK_SPACE: advance = true; break;
			case SDLK_a: step = !step; break;
			case SDLK_h: if (pause) { objq = comp(quaternion::axis_angle({ 0,1,0 }, +M_PI / 180), objq); } break;
			case SDLK_k: if (pause) { objq = comp(quaternion::axis_angle({ 0,1,0 }, -M_PI / 180), objq); } break;
			case SDLK_u: if (pause) { objq = comp(quaternion::axis_angle({ 1,0,0 }, -M_PI / 180), objq); } break;
			case SDLK_j: if (pause) { objq = comp(quaternion::axis_angle({ 1,0,0 }, +M_PI / 180), objq); } break;
			case SDLK_y: if (pause) { objq = comp(quaternion::axis_angle({ 0,0,1 }, -M_PI / 180), objq); } break;
			case SDLK_i: if (pause) { objq = comp(quaternion::axis_angle({ 0,0,1 }, +M_PI / 180), objq); } break;
			}
		}
	}


	// 	. if updated("frametime") set objects(obj,"t")=objects(obj,"a",objects(obj,"f"),"t") ;
	// 	. ;
	// 	. ;
	// 	. if save do
	// 	. . do %zecho(1)
	// 	. . write *27,"[3h",*27,"[2k"
	// 	. . write *27,"[2h",*27,"[2k"
	// 	. . set scene=$$zscrpromptandreadline(.ctl,20,"scene name: ","","","",scene,1)
	// 	. . do %zecho(0) ;
	// 	. . if ctl="uar" quit
	// 	. . do savedata(scene,.objects)
	// 	. . set save=0
	// 	. ;
	// 	. if load do
	// 	. . do %zecho(1)
	// 	. . do loadscene(.scene,.objects)
	// 	. . do %zecho(0)
	// 	. . set load=0
	// 	. ;
	// 	. if add do  ; add new keyframe
	// 	. . set add=0
	// 	. . set objects(obj,"a")=objects(obj,"a")+1
	// 	. . set objects(obj,"a",objects(obj,"a"),"p")="0 0 5" ; keyframe position
	// 	. . set objects(obj,"a",objects(obj,"a"),"q")=$$axisangle^xjtfq(0,1,0,0) ; keyframe quaternion
	// 	. . set objects(obj,"a",objects(obj,"a"),"t")=1 ; frametime
	// 	. . set objects(obj,"f")=objects(obj,"a")-1,objects(obj,"t")=0,next=1 ; select the frame
}




// 	;
// 	; DESCRIPTION: DISPLAY STATIC PARAMETER BAR
// 	; PARAMETERS:
// 	;   LINE   (I,REQ) - WHAT LINE OF THE SCREEN SHOULD THE PARAMETER BAR SHOULD BE DISPLAYED ON
// 	;   PARAMS (I,REQ) - A ; DELIMITED STRING OF ^ DELIMITED PARAMETER PROPERTY STRINGS. SEE PARSEPARAM FOR DETAILS
// STATUSBAR(LINE,PARAMS) ;
// 	NEW COLUMN,PARAM,NAME,INDEX
// 	WRITE *27,"[",LINE,"H" ; MOVE TO START OF LINE
// 	WRITE *27,"[0m" ; RESET ATTRIBUTES
// 	WRITE *27,"[2K" ; CLEAR LINE
// 	FOR INDEX=1:1:$LENGTH(PARAMS,";") DO
// 	. DO PARSEPARAM(PARAMS,INDEX,.PARAM,.NAME,.COLUMN) QUIT:PARAM=""
// 	. WRITE *27,"[",COLUMN,"G" ; MOVE TO COLUMN
// 	. WRITE $S(NAME'="":NAME,1:PARAM),"=",@PARAM
// 	. WRITE *27,"[0m"
// 	QUIT
// 	; DESCRIPTION: DISPLAY EDITABLE PARAMETER BAR
// 	; PARAMETERS:
// 	;   LINE     (I, REQ) - WHAT LINE OF THE SCREEN SHOULD THE PARAMETER BAR SHOULD BE DISPLAYED ON
// 	;   PARAMS   (I, REQ) - A ; DELIMITED STRING OF ^ DELIMITED PARAMETER PROPERTY STRINGS. SEE PARSEPARAM FOR DETAILS.
// 	;                       PARAM^NAME^COLUMN^INCREMENT^MIN^MAX^LINE
// 	;   SELECTED (IO,REQ) - INDEX OF CURRENTLY SELECTED PARAMETER, UPDATED VIA USER INPUT
// 	;   INPUT    (I, REQ) - BUFFER OF KEYBOARD INPUT THIS FRAME
// 	;   UPDATED  (O, OPT) - AN INDEX OF PARAMETERS THAT WERE UPDATED VIA USER INPUT
// PARAMSBAR(LINE,PARAMS,SELECTED,INPUT,UPDATED) ;
// 	NEW INDEX,PARAM,CHAR,KEY,COLUMN,INCREMENT,MIN,MAX,NAME,PLINE,I,PP
// 	; WRAP SELECTION
// 	IF SELECTED>$LENGTH(PARAMS,";") SET SELECTED=$LENGTH(PARAMS,";")
// 	;
// 	; HANDLE SELECTING/EDITING PARAMS WITH ARROW KEYS
// 	FOR I=1:1:INPUT SET CHAR=INPUT(I),KEY=INPUT(I,"KEY") DO
// 	. IF $CHAR(CHAR)="?" DO    ; DISPLAY HELP
// 	. . WRITE *27,"[1H"
// 	. . WRITE !,"         == CONTROLS ==         "
// 	. . WRITE !,"LEFT / RIGHT - SELECT PARAMETER "
// 	. . WRITE !,"   UP / DOWN - EDIT PARAMETER   "
// 	. . WRITE !,"           Q - QUIT             "
// 	. . WRITE !,"           ? - HELP             "
// 	. . READ !,"  (PRESS ANY KEY TO CONTINUE)   ",*%
// 	. . SET UPDATED=1
// 	. IF KEY=$$yLFARW() SET:SELECTED>1 SELECTED=SELECTED-1 QUIT
// 	. IF KEY=$$yRTARW() SET:SELECTED<$LENGTH(PARAMS,";") SELECTED=SELECTED+1 QUIT
// 	. DO PARSEPARAM(PARAMS,SELECTED,.PARAM,.NAME,.COLUMN,.INCREMENT,.MIN,.MAX)
// 	. IF PARAM="" QUIT
// 	. IF NAME="" SET NAME=PARAM
// 	. DO %zzStrSplitDelimList(PARAM,"$",.PARAM,.PP) IF PP="" SET PP=1
// 	. IF KEY=$$yUPARW() SET:(+INCREMENT'=0)&((MAX="")!($P(@PARAM," ",PP)+INCREMENT<=MAX)) $P(@PARAM," ",PP)=$P(@PARAM," ",PP)+INCREMENT,UPDATED(NAME)=1 QUIT
// 	. IF KEY=$$yDNARW() SET:(+INCREMENT'=0)&((MIN="")!($P(@PARAM," ",PP)-INCREMENT>=MIN)) $P(@PARAM," ",PP)=$P(@PARAM," ",PP)-INCREMENT,UPDATED(NAME)=1 QUIT
// 	;
// 	; PRINT PARAMETERS
// 	WRITE *27,"[",LINE,"H" ; MOVE TO START OF LINE
// 	WRITE *27,"[0m" ; RESET ATTRIBUTES
// 	WRITE *27,"[2K" ; CLEAR LINE
// 	FOR INDEX=1:1:$LENGTH(PARAMS,";") DO
// 	. DO PARSEPARAM(PARAMS,INDEX,.PARAM,.NAME,.COLUMN,"","","",.PLINE) QUIT:PARAM=""
// 	. DO %zzStrSplitDelimList(PARAM,"$",.PARAM,.PP) IF PP="" SET PP=1
// 	. WRITE *27,"[",LINE+PLINE,";",COLUMN,"H"    ; MOVE TO ROW, COLUMN
// 	. IF INDEX=SELECTED WRITE *27,"[7m"          ; HIGHLIGHT SELECTED
// 	. WRITE $S(NAME'="":NAME,1:PARAM),"=",$P(@PARAM," ",PP) ; WRITE THE PARAM AND VALUE
// 	. WRITE *27,"[0m"
// 	;
// 	QUIT
// HOTKEYBAR(LINE,PARAMS,INPUT) ;
// 	NEW I,CHAR,KEY,INDEX,PARAM,NAME,COLUMN,PLINE,PIX
// 	;
// 	FOR INDEX=1:1:$LENGTH(PARAMS,";") DO  ; INDEX PARAMETERS
// 	. DO PARSEPARAM(PARAMS,INDEX,.PARAM,.NAME) QUIT:PARAM=""
// 	. SET PIX($A($E($S(NAME'="":NAME,1:PARAM),1)))=PARAM
// 	;
// 	FOR I=1:1:INPUT SET CHAR=INPUT(I) DO  ; HANDLE INPUT
// 	. IF CHAR>96 SET CHAR=CHAR-32 ; CONVERT TO UPPERCASE
// 	. SET PARAM=PIX(CHAR) QUIT:PARAM=""
// 	. SET @PARAM='@PARAM ; TOGGLE PARAM ON KEYPRESS
// 	;
// 	; PRINT PARAMETERS
// 	WRITE *27,"[",LINE,"H" ; MOVE TO START OF LINE
// 	WRITE *27,"[0m" ; RESET ATTRIBUTES
// 	WRITE *27,"[2K" ; CLEAR LINE
// 	FOR INDEX=1:1:$LENGTH(PARAMS,";") DO
// 	. DO PARSEPARAM(PARAMS,INDEX,.PARAM,.NAME,.COLUMN,"","","",.PLINE) QUIT:PARAM=""
// 	. IF COLUMN'="" WRITE *27,"[",LINE+PLINE,";",COLUMN,"H" ; MOVE TO ROW, COLUMN, IF SPECIFIED
// 	. IF @PARAM WRITE *27,"[7m" ; HIGHLIGHT IF TOGGLED
// 	. IF NAME="" SET NAME=PARAM
// 	. WRITE *27,"[4m",$E(NAME,1),*27,"[24m",$E(NAME,2,$L(NAME)) ; WRITE THE PARAM WITH FIRST CHARACTER UNDERLINED
// 	. WRITE *27,"[0m "
// 	. ;
// 	QUIT
// 	;
// 	;

// 	;
// 	;
